/**
 * @file Response.h
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

struct Response
{
    http_status status;
    std::unordered_map<std::string, std::string> headers;
    Kite::ByteStream body;
};

} // namespace Kite::HTTP