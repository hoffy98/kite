/**
 * @file Uart.cpp
 * @author hoffy98
 * @brief
 * @date 2026-05-08
 */
#include <Kite/Serial/Uart.h>

#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(kite_serial_uart, CONFIG_KITE_SERIAL_LOG_LEVEL);

namespace Kite::Serial
{

Uart::Uart(const device* uart_dev)
    : m_uart_device(uart_dev), m_rx_callback(nullptr)
{
    if (m_uart_device == nullptr)
    {
        LOG_ERR("UART device pointer is null");
    }

    if (!device_is_ready(m_uart_device))
    {
        LOG_ERR("UART device is not ready");
    }
}

int Uart::SetConfig(const uart_config& config)
{
    int rc;

    rc = uart_configure(m_uart_device, &config);
    if (rc != 0)
    {
        LOG_ERR("Failed to configure UART: %d", rc);
        return rc;
    }

    return 0;
}

int Uart::GetConfig(uart_config& config) const
{
    int rc;

    rc = uart_config_get(m_uart_device, &config);
    if (rc != 0)
    {
        LOG_ERR("Failed to get UART configuration: %d", rc);
        return rc;
    }

    return 0;
}

void Uart::SetRxCallback(RxCallback_t callback)
{
    m_rx_callback = callback;
}

} // namespace Kite::Serial