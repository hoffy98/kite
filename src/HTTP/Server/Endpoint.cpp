/**
 * @file Endpoint.cpp
 * @author hoffy98
 * @brief
 * @date 2026-05-01
 */
#include <Kite/HTTP/Server/Endpoint.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(kite_http_endpoint, CONFIG_KITE_HTTP_LOG_LEVEL);

#define HTTP_REQ_RES_BUF_SIZE 2048

namespace Kite::HTTP::Endpoint
{

static HTTP::Request req = { .method  = HTTP_GET,
                             .uri     = "/",
                             .headers = {},
                             .body = Kite::ByteStream(HTTP_REQ_RES_BUF_SIZE) };

static HTTP::Response res = { .status  = HTTP_200_OK,
                              .headers = {},
                              .body = Kite::ByteStream(HTTP_REQ_RES_BUF_SIZE) };

static const char* http_method_strings[] = { "DELETE", "GET", "HEAD",
                                             "POST",   "PUT", "CONNECT",
                                             "OPTIONS" };

int Callback(struct http_client_ctx* client,
             enum http_transaction_status status,
             const struct http_request_ctx* requestContext,
             struct http_response_ctx* responseContext,
             void* userData)
{
    Data* data = static_cast<Data*>(userData);
    if (data == nullptr || data->handler == nullptr)
    {
        responseContext->status      = HTTP_500_INTERNAL_SERVER_ERROR;
        responseContext->final_chunk = true;
        return 0;
    }

    if (status == HTTP_SERVER_TRANSACTION_ABORTED)
    {
        req.body.Clear();
        return 0;
    }

    if (status == HTTP_SERVER_TRANSACTION_COMPLETE)
    {
        req.body.Clear();
        res.body.Clear();
        res.headers.clear();
        return 0;
    }

    if (status == HTTP_SERVER_REQUEST_DATA_MORE || status == HTTP_SERVER_REQUEST_DATA_FINAL)
    {
        if (requestContext->data_len > 0)
        {
            int ret = req.body.Write(requestContext->data, requestContext->data_len);
            if (ret != 0)
            {
                LOG_ERR("Request body exceeds buffer");
                responseContext->status      = HTTP_413_PAYLOAD_TOO_LARGE;
                responseContext->final_chunk = true;
                req.body.Clear();
                return 0;
            }
        }

        if (status == HTTP_SERVER_REQUEST_DATA_MORE)
        {
            return 0;
        }
    }

    // HTTP_SERVER_REQUEST_DATA_FINAL: populate req and dispatch to handler
    req.method = client->method;
    req.uri.assign(reinterpret_cast<const char*>(client->url_buffer));

    req.headers.clear();
    for (size_t i = 0; i < requestContext->header_count; ++i)
    {
        req.headers[requestContext->headers[i].name] =
            requestContext->headers[i].value;
    }

    res.body.Clear();
    res.headers.clear();
    res.status = HTTP_200_OK;

    LOG_DBG("Dispatching request: method=%s, uri=%s, body_len=%zu",
            http_method_strings[req.method], req.uri.c_str(), req.body.GetSize());

    data->handler(req, res);

    req.body.Clear();

    // Map response headers into a static array for Zephyr
    static struct http_header responseHeaders[8];
    size_t headerCount = 0;
    for (const auto& [name, value] : res.headers)
    {
        if (headerCount >= ARRAY_SIZE(responseHeaders))
        {
            break;
        }
        responseHeaders[headerCount].name  = name.c_str();
        responseHeaders[headerCount].value = value.c_str();
        ++headerCount;
    }

    responseContext->headers      = responseHeaders;
    responseContext->header_count = headerCount;
    responseContext->body         = res.body.GetData();
    responseContext->body_len     = res.body.GetSize();
    responseContext->status       = res.status;
    responseContext->final_chunk  = true;

    return 0;
}

} // namespace Kite::HTTP::Endpoint