/**
 * @file UartAsync.cpp
 * @author hoffy98
 * @brief UART driver using the Zephyr async UART API with double-buffered RX.
 * @date 2026-05-08
 */
#include <Kite/Serial/UartAsync.h>

#include <Kite/ByteStream.h>
#include <string.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(kite_serial_uart_async, CONFIG_KITE_SERIAL_LOG_LEVEL);

namespace Kite::Serial
{

UartAsync::UartAsync(const device* uart_dev)
    : Uart(uart_dev)
    , m_rx_buf{ std::make_unique<uint8_t[]>(RX_BUF_SIZE),
                std::make_unique<uint8_t[]>(RX_BUF_SIZE) }
    , m_tx_buf(std::make_unique<uint8_t[]>(TX_BUF_SIZE))
    , m_rx_next_buf(0)
{
    int rc = uart_callback_set(m_uart_device, UartAsync::UartCallback, this);
    if (rc != 0)
    {
        LOG_ERR("Failed to register UART async callback: %d", rc);
    }
}

UartAsync::~UartAsync()
{
    uart_rx_disable(m_uart_device);
}

int UartAsync::Write(const uint8_t* data, size_t length)
{
    if (length > TX_BUF_SIZE)
    {
        LOG_ERR("uart_tx: data length %zu exceeds TX buffer size %zu", length, TX_BUF_SIZE);
        return -ENOMEM;
    }
    memcpy(m_tx_buf.get(), data, length);
    int rc = uart_tx(m_uart_device, m_tx_buf.get(), length, SYS_FOREVER_US);
    if (rc != 0)
    {
        LOG_ERR("uart_tx failed: %d", rc);
    }
    return rc;
}

int UartAsync::StartRx()
{
    m_rx_next_buf = 1;
    int rc = uart_rx_enable(m_uart_device, m_rx_buf[0].get(), RX_BUF_SIZE, 0);
    if (rc != 0)
    {
        LOG_ERR("uart_rx_enable failed: %d", rc);
    }
    return rc;
}

void UartAsync::StopRx()
{
    int rc = uart_rx_disable(m_uart_device);
    if (rc != 0)
    {
        LOG_ERR("uart_rx_disable failed: %d", rc);
    }
}

void UartAsync::UartCallback(const device* dev, uart_event* evt, void* user_data)
{
    ARG_UNUSED(dev);
    static_cast<UartAsync*>(user_data)->OnUartEvent(evt);
}

void UartAsync::OnUartEvent(const uart_event* evt)
{
    switch (evt->type)
    {
    case UART_TX_DONE:
        LOG_DBG("TX done: %zu bytes", evt->data.tx.len);
        break;

    case UART_TX_ABORTED:
        LOG_WRN("TX aborted after %zu bytes", evt->data.tx.len);
        break;

    case UART_RX_RDY:
        if (m_rx_callback != nullptr)
        {
            const uart_event_rx& rx = evt->data.rx;
            Kite::ByteStream stream(rx.buf + rx.offset, rx.len, rx.len);
            m_rx_callback(stream);
        }
        break;

    case UART_RX_BUF_REQUEST:
        uart_rx_buf_rsp(m_uart_device, m_rx_buf[m_rx_next_buf].get(), RX_BUF_SIZE);
        m_rx_next_buf ^= 1u;
        break;

    case UART_RX_BUF_RELEASED:
        break;

    case UART_RX_DISABLED:
        LOG_DBG("RX disabled");
        break;

    case UART_RX_STOPPED:
        LOG_WRN("RX stopped, reason: %d", evt->data.rx_stop.reason);
        break;

    default:
        break;
    }
}

} // namespace Kite::Serial