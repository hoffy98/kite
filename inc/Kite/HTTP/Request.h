/**
 * @file Request.h
 * @author hoffy98
 * @brief
 * @date 2026-05-01
 */
#pragma once
#include <Kite/ByteStream.h>
#include <Kite/HTTP/Server.h>

#include <string>
#include <unordered_map>

namespace Kite::HTTP
{

struct Request
{
    http_method method;
    std::string uri;
    std::unordered_map<std::string, std::string> headers;
    Kite::ByteStream body;
};

} // namespace Kite::HTTP