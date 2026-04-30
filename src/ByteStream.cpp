/**
 * @file ByteStream.cpp
 * @author hoffy98
 * @brief
 * @date 2026-04-29
 */
#include "kite/ByteStream.h"

#include <zephyr/logging/log.h>

#include <cstring>

LOG_MODULE_REGISTER(kite, CONFIG_KITE_LOG_LEVEL);

namespace Kite
{

// -------------------------------------------------------------------------
// Construction / Destruction
// -------------------------------------------------------------------------

ByteStream::ByteStream()
: m_data(new uint8_t[CONFIG_KITE_BYTE_STREAM_DEFAULT_BUFFER_SIZE]{}),
  m_capacity(CONFIG_KITE_BYTE_STREAM_DEFAULT_BUFFER_SIZE), m_writePos(0),
  m_readPos(0), m_ownsData(true), m_endian(Endian::Little)
{
}

ByteStream::ByteStream(size_t capacity)
: m_data(capacity > 0 ? new uint8_t[capacity]{} : nullptr), m_capacity(capacity),
  m_writePos(0), m_readPos(0), m_ownsData(true), m_endian(Endian::Little)
{
}

ByteStream::ByteStream(uint8_t* data, size_t capacity)
: m_data(data), m_capacity(capacity), m_writePos(0), m_readPos(0),
  m_ownsData(false), m_endian(Endian::Little)
{
}

ByteStream::ByteStream(uint8_t* data, size_t capacity, size_t size)
: m_data(data), m_capacity(capacity), m_writePos(size <= capacity ? size : capacity),
  m_readPos(0), m_ownsData(false), m_endian(Endian::Little)
{
}

ByteStream::ByteStream(const ByteStream& other)
: m_data(other.m_capacity > 0 ? new uint8_t[other.m_capacity] : nullptr),
  m_capacity(other.m_capacity), m_writePos(other.m_writePos),
  m_readPos(other.m_readPos), m_ownsData(true), m_endian(other.m_endian)
{
    if (m_data && other.m_data)
    {
        memcpy(m_data, other.m_data, m_capacity);
    }
}

ByteStream::ByteStream(ByteStream&& other) noexcept
: m_data(other.m_data), m_capacity(other.m_capacity), m_writePos(other.m_writePos),
  m_readPos(other.m_readPos), m_ownsData(other.m_ownsData), m_endian(other.m_endian)
{
    other.m_data     = nullptr;
    other.m_capacity = 0;
    other.m_writePos = 0;
    other.m_readPos  = 0;
    other.m_ownsData = false;
}

ByteStream::~ByteStream()
{
    if (m_ownsData && m_data)
    {
        delete[] m_data;
        m_data = nullptr;
    }
}

// -------------------------------------------------------------------------
// Assignment
// -------------------------------------------------------------------------

ByteStream& ByteStream::operator=(const ByteStream& other)
{
    if (this == &other)
        return *this;

    if (m_ownsData)
    {
        delete[] m_data;
    }

    m_capacity = other.m_capacity;
    m_writePos = other.m_writePos;
    m_readPos  = other.m_readPos;
    m_endian   = other.m_endian;
    m_ownsData = true;

    if (m_capacity > 0)
    {
        m_data = new uint8_t[m_capacity];
        if (other.m_data)
        {
            memcpy(m_data, other.m_data, m_capacity);
        }
    }
    else
    {
        m_data = nullptr;
    }

    return *this;
}

ByteStream& ByteStream::operator=(ByteStream&& other) noexcept
{
    if (this == &other)
        return *this;

    if (m_ownsData)
    {
        delete[] m_data;
    }

    m_data     = other.m_data;
    m_capacity = other.m_capacity;
    m_writePos = other.m_writePos;
    m_readPos  = other.m_readPos;
    m_ownsData = other.m_ownsData;
    m_endian   = other.m_endian;

    other.m_data     = nullptr;
    other.m_capacity = 0;
    other.m_writePos = 0;
    other.m_readPos  = 0;
    other.m_ownsData = false;

    return *this;
}

// -------------------------------------------------------------------------
// Capacity & State
// -------------------------------------------------------------------------

const uint8_t* ByteStream::GetData() const
{
    return m_data;
}
uint8_t* ByteStream::GetData()
{
    return m_data;
}
size_t ByteStream::GetCapacity() const
{
    return m_capacity;
}
size_t ByteStream::GetSize() const
{
    return m_writePos;
}
size_t ByteStream::GetReadPos() const
{
    return m_readPos;
}
size_t ByteStream::GetWritePos() const
{
    return m_writePos;
}
size_t ByteStream::GetAvailable() const
{
    return m_writePos - m_readPos;
}
size_t ByteStream::GetFreeSpace() const
{
    return m_capacity - m_writePos;
}
bool ByteStream::IsEmpty() const
{
    return m_writePos == 0;
}
bool ByteStream::IsFull() const
{
    return m_writePos >= m_capacity;
}
bool ByteStream::IsReadable() const
{
    return m_readPos < m_writePos;
}
bool ByteStream::IsWritable() const
{
    return m_writePos < m_capacity;
}
bool ByteStream::OwnsData() const
{
    return m_ownsData;
}
void ByteStream::SetEndian(Endian endian)
{
    m_endian = endian;
}
Endian ByteStream::GetEndian() const
{
    return m_endian;
}

// -------------------------------------------------------------------------
// Position Control
// -------------------------------------------------------------------------

void ByteStream::Clear()
{
    if (m_data)
    {
        memset(m_data, 0, m_capacity);
    }
    m_writePos = 0;
    m_readPos  = 0;
}

void ByteStream::ResetRead()
{
    m_readPos = 0;
}
void ByteStream::ResetWrite()
{
    m_writePos = 0;
}

int ByteStream::SeekRead(size_t pos)
{
    if (pos > m_writePos)
        return -EINVAL;
    m_readPos = pos;
    return 0;
}

int ByteStream::SeekWrite(size_t pos)
{
    if (pos > m_capacity)
        return -EINVAL;
    m_writePos = pos;
    return 0;
}

int ByteStream::SkipRead(size_t bytes)
{
    if (bytes > GetAvailable())
        return -ENODATA;
    m_readPos += bytes;
    return 0;
}

int ByteStream::SkipWrite(size_t bytes)
{
    if (bytes > GetFreeSpace())
        return -ENOSPC;
    m_writePos += bytes;
    return 0;
}

// -------------------------------------------------------------------------
// Write
// -------------------------------------------------------------------------

int ByteStream::Write(const uint8_t* data, size_t size)
{
    if (GetFreeSpace() < size)
        return -ENOSPC;
    memcpy(m_data + m_writePos, data, size);
    m_writePos += size;
    return 0;
}

int ByteStream::Write(const char* str)
{
    if (!str)
        return -EINVAL;
    return Write(reinterpret_cast<const uint8_t*>(str), strlen(str));
}

int ByteStream::Write(const ByteStream& other)
{
    return Write(other.m_data, other.m_writePos);
}

int ByteStream::Write(bool value)
{
    uint8_t b = value ? 1u : 0u;
    return Write(&b, 1);
}

int ByteStream::Write(uint8_t value)
{
    return Write(&value, 1);
}

int ByteStream::Write(uint16_t value)
{
    if (GetFreeSpace() < 2)
        return -ENOSPC;
    uint8_t* p = m_data + m_writePos;
    if (m_endian == Endian::Little)
    {
        p[0] = static_cast<uint8_t>(value);
        p[1] = static_cast<uint8_t>(value >> 8);
    }
    else
    {
        p[0] = static_cast<uint8_t>(value >> 8);
        p[1] = static_cast<uint8_t>(value);
    }
    m_writePos += 2;
    return 0;
}

int ByteStream::Write(uint32_t value)
{
    if (GetFreeSpace() < 4)
        return -ENOSPC;
    uint8_t* p = m_data + m_writePos;
    if (m_endian == Endian::Little)
    {
        p[0] = static_cast<uint8_t>(value);
        p[1] = static_cast<uint8_t>(value >> 8);
        p[2] = static_cast<uint8_t>(value >> 16);
        p[3] = static_cast<uint8_t>(value >> 24);
    }
    else
    {
        p[0] = static_cast<uint8_t>(value >> 24);
        p[1] = static_cast<uint8_t>(value >> 16);
        p[2] = static_cast<uint8_t>(value >> 8);
        p[3] = static_cast<uint8_t>(value);
    }
    m_writePos += 4;
    return 0;
}

int ByteStream::Write(uint64_t value)
{
    if (GetFreeSpace() < 8)
        return -ENOSPC;
    uint8_t* p = m_data + m_writePos;
    if (m_endian == Endian::Little)
    {
        for (int i = 0; i < 8; ++i)
        {
            p[i] = static_cast<uint8_t>(value >> (i * 8));
        }
    }
    else
    {
        for (int i = 0; i < 8; ++i)
        {
            p[i] = static_cast<uint8_t>(value >> ((7 - i) * 8));
        }
    }
    m_writePos += 8;
    return 0;
}

int ByteStream::Write(int8_t value)
{
    return Write(static_cast<uint8_t>(value));
}
int ByteStream::Write(int16_t value)
{
    return Write(static_cast<uint16_t>(value));
}
int ByteStream::Write(int32_t value)
{
    return Write(static_cast<uint32_t>(value));
}
int ByteStream::Write(int64_t value)
{
    return Write(static_cast<uint64_t>(value));
}

int ByteStream::Write(float value)
{
    uint32_t raw;
    memcpy(&raw, &value, sizeof(raw));
    return Write(raw);
}

int ByteStream::Write(double value)
{
    uint64_t raw;
    memcpy(&raw, &value, sizeof(raw));
    return Write(raw);
}

// -------------------------------------------------------------------------
// Read
// -------------------------------------------------------------------------

int ByteStream::Read(uint8_t* buffer, size_t size)
{
    if (GetAvailable() < size)
        return -ENODATA;
    memcpy(buffer, m_data + m_readPos, size);
    m_readPos += size;
    return 0;
}

int ByteStream::Read(char* buffer, size_t size)
{
    return Read(reinterpret_cast<uint8_t*>(buffer), size);
}

int ByteStream::Read(ByteStream& other)
{
    size_t avail = GetAvailable();
    if (other.GetFreeSpace() < avail)
        return -ENOSPC;
    memcpy(other.m_data + other.m_writePos, m_data + m_readPos, avail);
    other.m_writePos += avail;
    m_readPos += avail;
    return 0;
}

int ByteStream::Read(bool& value)
{
    uint8_t b;
    int ret = Read(&b, 1);
    if (ret == 0)
        value = (b != 0);
    return ret;
}

int ByteStream::Read(uint8_t& value)
{
    return Read(&value, 1);
}

int ByteStream::Read(uint16_t& value)
{
    if (GetAvailable() < 2)
        return -ENODATA;
    const uint8_t* p = m_data + m_readPos;
    if (m_endian == Endian::Little)
    {
        value = static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
    }
    else
    {
        value = (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
    }
    m_readPos += 2;
    return 0;
}

int ByteStream::Read(uint32_t& value)
{
    if (GetAvailable() < 4)
        return -ENODATA;
    const uint8_t* p = m_data + m_readPos;
    if (m_endian == Endian::Little)
    {
        value = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
        (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    }
    else
    {
        value = (static_cast<uint32_t>(p[0]) << 24) |
        (static_cast<uint32_t>(p[1]) << 16) |
        (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
    }
    m_readPos += 4;
    return 0;
}

int ByteStream::Read(uint64_t& value)
{
    if (GetAvailable() < 8)
        return -ENODATA;
    const uint8_t* p = m_data + m_readPos;
    value            = 0;
    if (m_endian == Endian::Little)
    {
        for (int i = 0; i < 8; ++i)
        {
            value |= static_cast<uint64_t>(p[i]) << (i * 8);
        }
    }
    else
    {
        for (int i = 0; i < 8; ++i)
        {
            value |= static_cast<uint64_t>(p[i]) << ((7 - i) * 8);
        }
    }
    m_readPos += 8;
    return 0;
}

int ByteStream::Read(int8_t& value)
{
    return Read(reinterpret_cast<uint8_t&>(value));
}
int ByteStream::Read(int16_t& value)
{
    return Read(reinterpret_cast<uint16_t&>(value));
}
int ByteStream::Read(int32_t& value)
{
    return Read(reinterpret_cast<uint32_t&>(value));
}
int ByteStream::Read(int64_t& value)
{
    return Read(reinterpret_cast<uint64_t&>(value));
}

int ByteStream::Read(float& value)
{
    uint32_t raw;
    int ret = Read(raw);
    if (ret == 0)
        memcpy(&value, &raw, sizeof(value));
    return ret;
}

int ByteStream::Read(double& value)
{
    uint64_t raw;
    int ret = Read(raw);
    if (ret == 0)
        memcpy(&value, &raw, sizeof(value));
    return ret;
}

// -------------------------------------------------------------------------
// Peek
// -------------------------------------------------------------------------

int ByteStream::Peek(uint8_t* buffer, size_t size) const
{
    if (GetAvailable() < size)
        return -ENODATA;
    memcpy(buffer, m_data + m_readPos, size);
    return 0;
}

int ByteStream::Peek(uint8_t& value) const
{
    if (!IsReadable())
        return -ENODATA;
    value = m_data[m_readPos];
    return 0;
}

int ByteStream::Peek(uint16_t& value) const
{
    if (GetAvailable() < 2)
        return -ENODATA;
    const uint8_t* p = m_data + m_readPos;
    if (m_endian == Endian::Little)
    {
        value = static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
    }
    else
    {
        value = (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
    }
    return 0;
}

int ByteStream::Peek(uint32_t& value) const
{
    if (GetAvailable() < 4)
        return -ENODATA;
    const uint8_t* p = m_data + m_readPos;
    if (m_endian == Endian::Little)
    {
        value = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
        (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    }
    else
    {
        value = (static_cast<uint32_t>(p[0]) << 24) |
        (static_cast<uint32_t>(p[1]) << 16) |
        (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
    }
    return 0;
}

int ByteStream::PeekAt(size_t offset, uint8_t* buffer, size_t size) const
{
    if (offset >= m_capacity)
        return -EINVAL;
    if (offset + size > m_writePos)
        return -ENODATA;
    memcpy(buffer, m_data + offset, size);
    return 0;
}

int ByteStream::PeekAt(size_t offset, uint8_t& value) const
{
    if (offset >= m_writePos)
        return -EINVAL;
    value = m_data[offset];
    return 0;
}

// -------------------------------------------------------------------------
// Operators
// -------------------------------------------------------------------------

uint8_t ByteStream::operator[](size_t index) const
{
    return m_data[index];
}
uint8_t& ByteStream::operator[](size_t index)
{
    return m_data[index];
}

bool ByteStream::operator==(const ByteStream& other) const
{
    if (m_writePos != other.m_writePos)
        return false;
    if (m_writePos == 0)
        return true;
    return memcmp(m_data, other.m_data, m_writePos) == 0;
}

bool ByteStream::operator!=(const ByteStream& other) const
{
    return !(*this == other);
}

ByteStream::operator bool() const
{
    return IsReadable();
}

} // namespace Kite