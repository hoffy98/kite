/**
 * @file Server.h
 * @author hoffy98
 * @brief
 * @date 2026-05-01
 */
#pragma once
#include <zephyr/net/http/server.h>
#include <zephyr/net/http/service.h>

extern uint16_t http_service_port;
extern const http_service_desc http_service_descriptor;

namespace Kite::HTTP::Server
{

int Init();

} // namespace Kite::HTTP::Server