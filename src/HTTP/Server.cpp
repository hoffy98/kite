/**
 * @file Server.cpp
 * @author hoffy98
 * @brief
 * @date 2026-05-01
 */
#include <Kite/HTTP/Server.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(kite_http_server, CONFIG_KITE_HTTP_LOG_LEVEL);

uint16_t http_service_port = 80;
HTTP_SERVICE_DEFINE(http_service_descriptor, NULL, &http_service_port, 1, 2, NULL, NULL, NULL);

namespace Kite::HTTP::Server
{

int Init()
{
    http_server_start();
    LOG_INF("HTTP server started on port %d", http_service_port);
    return 0;
}

} // namespace Kite::HTTP::Server