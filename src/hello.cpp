/**
 * @file hello.cpp
 * @author hoffy98
 * @brief 
 * @date 2026-04-29
 */
#include "kite/hello.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(kite, CONFIG_KITE_LOG_LEVEL);

void hello()
{
    LOG_INF("Hello from the Kite library!");
}