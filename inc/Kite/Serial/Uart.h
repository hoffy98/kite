/**
 * @file Uart.h
 * @author hoffy98
 * @brief
 * @date 2026-05-08
 */
#pragma once
#include <Kite/ByteStream.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

namespace Kite::Serial
{

class Uart
{
public:
    using RxCallback_t = void (*)(const Kite::ByteStream& data);

    Uart(const device* uart_dev);
    virtual ~Uart() = default;

    int SetConfig(const uart_config& config);
    int GetConfig(uart_config& config) const;

    virtual int Write(const uint8_t* data, size_t length) = 0;
    int Write(const char* str);
    int Write(const Kite::ByteStream& data);

    void SetRxCallback(RxCallback_t callback);

protected:
    const device* m_uart_device;
    RxCallback_t m_rx_callback;
};

inline int Uart::Write(const Kite::ByteStream& data)
{
    return Write(data.GetData(), data.GetSize());
}

inline int Uart::Write(const char* str)
{
    return Write((const uint8_t*)str, strlen(str));
}

} // namespace Kite::Serial