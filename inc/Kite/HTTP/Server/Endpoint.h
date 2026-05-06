/**
 * @file Endpoint.h
 * @author hoffy98
 * @brief
 * @date 2026-05-01
 */
#pragma once
#include <Kite/HTTP/Request.h>
#include <Kite/HTTP/Response.h>

namespace Kite::HTTP::Endpoint
{

using Handler = void (*)(const Request&, Response&);

struct Data
{
    Handler handler;
};

int Callback(struct http_client_ctx* client,
             enum http_transaction_status status,
             const struct http_request_ctx* requestContext,
             struct http_response_ctx* responseContext,
             void* userData);

} // namespace Kite::HTTP::Endpoint