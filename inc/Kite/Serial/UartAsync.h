/**
 * @file UartAsync.h
 * @author hoffy98
 * @brief UART driver using the Zephyr async UART API with double-buffered RX.
 * @date 2026-05-08
 */
#pragma once
#include <Kite/Serial/Uart.h>

#include <memory>
#include <zephyr/drivers/uart.h>

namespace Kite::Serial
{

class UartAsync : public Uart
{
public:
    UartAsync(const device* uart_dev);
    ~UartAsync() override;

    using Uart::Write;
    int Write(const uint8_t* data, size_t length) override;

    int StartRx();
    void StopRx();

private:
    static void UartCallback(const device* dev,
                             uart_event* evt,
                             void* user_data);
    void OnUartEvent(const uart_event* evt);

    static constexpr size_t RX_BUF_SIZE =
        CONFIG_KITE_SERIAL_UART_ASYNC_RX_BUF_SIZE;
    static constexpr size_t TX_BUF_SIZE =
        CONFIG_KITE_SERIAL_UART_ASYNC_TX_BUF_SIZE;

    std::unique_ptr<uint8_t[]> m_rx_buf[2];
    std::unique_ptr<uint8_t[]> m_tx_buf;
    uint8_t m_rx_next_buf;
};

} // namespace Kite::Serial