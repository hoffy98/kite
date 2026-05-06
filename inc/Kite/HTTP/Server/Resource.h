/**
 * @file Resource.h
 * @author hoffy98
 * @brief
 * @date 2026-05-01
 */
#pragma once
#include <Kite/HTTP/Server/Endpoint.h>

#define GET     BIT(HTTP_GET)
#define POST    BIT(HTTP_POST)
#define PUT     BIT(HTTP_PUT)
#define DELETE  BIT(HTTP_DELETE)

/**
 * @brief Macro to add an HTTP resource.
 * @param NAME The name of the resource (used for variable names).
 * @param METHODS A bitmask of supported HTTP methods (e.g., GET | POST).
 * @param URI The URI path for the resource (e.g., "/echo").
 * @param CONTENT_TYPE The content type for the resource (e.g., "text/plain").
 * @param HANDLER The handler function for the resource (e.g., echo_handler).
 */
#define KITE_HTTP_RESOURCE_DEFINE(NAME, METHODS, URI, CONTENT_TYPE, HANDLER)            \
    static Kite::HTTP::Endpoint::Data NAME##_endpoint_data = { .handler = HANDLER };    \
    static struct http_resource_detail_dynamic NAME##_resource = {                      \
        .common = {                                                                     \
            .bitmask_of_supported_http_methods = METHODS,                               \
            .type = HTTP_RESOURCE_TYPE_DYNAMIC,                                         \
            .path_len = sizeof(URI) - 1,                                                \
            .content_encoding = NULL,                                                   \
            .content_type = CONTENT_TYPE,                                               \
        },                                                                              \
        .cb = Kite::HTTP::Endpoint::Callback,                                           \
        .holder = NULL,                                                                 \
        .user_data = &NAME##_endpoint_data,                                             \
    };                                                                                  \
    HTTP_RESOURCE_DEFINE(NAME##_http, http_service_descriptor, URI, &NAME##_resource);