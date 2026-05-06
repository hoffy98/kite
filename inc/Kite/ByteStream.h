/**
 * @file ByteStream.h
 * @author hoffy98
 * @brief ByteStream class for managing linear byte buffers with independent read and write cursors.
 * @date 2026-04-29
 */
#pragma once
#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace Kite
{

enum class Endian : uint8_t
{
    Little,
    Big
};

class ByteStream
{
    public:
    // -------------------------------------------------------------------------
    // Construction / Destruction
    // -------------------------------------------------------------------------

    /**
     * @brief Constructs an ByteStream with KITE_BYTE_STREAM_DEFAULT_BUFFER_SIZE bytes of heap-allocated storage.
     * The buffer is owned and managed by this instance.
     */
    ByteStream();

    /**
     * @brief Constructs a ByteStream with a heap-allocated buffer of the given capacity.
     * @param capacity Number of bytes to allocate.
     */
    explicit ByteStream(size_t capacity);

    /**
     * @brief Constructs a non-owning ByteStream wrapping an existing external
     * buffer. The caller is responsible for the lifetime of @p data.
     * @param data Pointer to the external buffer.
     * @param capacity Capacity of the external buffer in bytes.
     */
    ByteStream(uint8_t* data, size_t capacity);

    /**
     * @brief Constructs a ByteStream with the given external buffer and capacity.
     * The caller is responsible for the lifetime of @p data. The write cursor
     * is initialized to @p size, indicating that the buffer is pre-filled with
     * @p size bytes of valid data.
     * @param data Pointer to the external buffer.
     * @param capacity Total capacity of the external buffer in bytes.
     * @param size Number of bytes already written into the buffer (must be <= capacity).
     */
    ByteStream(uint8_t* data, size_t capacity, size_t size);

    /**
     * @brief Copy constructor. Performs a deep copy of @p other's buffer and state.
     * @param other The ByteStream to copy from.
     */
    ByteStream(const ByteStream& other);

    /**
     * @brief Move constructor. Transfers ownership of @p other's buffer.
     *        After the move, @p other is left in a valid but empty state.
     * @param other The ByteStream to move from.
     */
    ByteStream(ByteStream&& other) noexcept;

    /**
     * @brief Destructor. Releases the heap-allocated buffer if owned.
     */
    ~ByteStream();

    // -------------------------------------------------------------------------
    // Assignment
    // -------------------------------------------------------------------------

    /**
     * @brief Copy assignment. Performs a deep copy of @p other's buffer and state.
     * @param other The ByteStream to copy from.
     * @return Reference to this ByteStream.
     */
    ByteStream& operator=(const ByteStream& other);

    /**
     * @brief Move assignment. Transfers ownership of @p other's buffer.
     *        After the move, @p other is left in a valid but empty state.
     * @param other The ByteStream to move from.
     * @return Reference to this ByteStream.
     */
    ByteStream& operator=(ByteStream&& other) noexcept;

    // -------------------------------------------------------------------------
    // Capacity & State
    // -------------------------------------------------------------------------

    /**
     * @brief Returns a read-only pointer to the start of the internal buffer.
     * @return Const pointer to the buffer, or nullptr if no buffer is allocated.
     */
    const uint8_t* GetData() const;

    /**
     * @brief Returns a writable pointer to the start of the internal buffer.
     * @return Pointer to the buffer, or nullptr if no buffer is allocated.
     */
    uint8_t* GetData();

    /**
     * @brief Returns the total capacity of the internal buffer in bytes.
     * @return Buffer capacity in bytes.
     */
    size_t GetCapacity() const;

    /**
     * @brief Returns the number of bytes that have been written (equals the write cursor position).
     * @return Number of written bytes.
     */
    size_t GetSize() const;

    /**
     * @brief Returns the current read cursor position.
     * @return Read cursor offset from the start of the buffer.
     */
    size_t GetReadPos() const;

    /**
     * @brief Returns the current write cursor position.
     * @return Write cursor offset from the start of the buffer.
     */
    size_t GetWritePos() const;

    /**
     * @brief Returns the number of bytes available to read (written bytes not yet consumed).
     * @return Bytes between the read cursor and the write cursor.
     */
    size_t GetAvailable() const;

    /**
     * @brief Returns the number of bytes that can still be written before the buffer is full.
     * @return Bytes between the write cursor and the end of the buffer.
     */
    size_t GetFreeSpace() const;

    /**
     * @brief Checks whether no bytes have been written to the stream.
     * @return true if the write cursor is at position 0.
     */
    bool IsEmpty() const;

    /**
     * @brief Checks whether the write cursor has reached the end of the buffer.
     * @return true if no free space remains.
     */
    bool IsFull() const;

    /**
     * @brief Checks whether there is at least one byte available to read.
     * @return true if the read cursor is behind the write cursor.
     */
    bool IsReadable() const;

    /**
     * @brief Checks whether there is at least one byte of free space to write.
     * @return true if the write cursor has not reached the buffer capacity.
     */
    bool IsWritable() const;

    /**
     * @brief Checks whether this ByteStream owns and manages its buffer memory.
     * @return true if the buffer was heap-allocated by this instance.
     */
    bool OwnsData() const;

    /**
     * @brief Sets the byte order used for multi-byte integer Read and Write
     * operations. Defaults to Endian::Little on construction.
     * @param endian The desired byte order.
     */
    void SetEndian(Endian endian);

    /**
     * @brief Returns the currently configured byte order.
     * @return The active Endian setting.
     */
    Endian GetEndian() const;

    // -------------------------------------------------------------------------
    // Position Control
    // -------------------------------------------------------------------------

    /**
     * @brief Resets both the read and write cursors to 0 and zeroes the buffer contents.
     */
    void Clear();

    /**
     * @brief Rewinds the read cursor to the beginning of the buffer without modifying data.
     */
    void ResetRead();

    /**
     * @brief Rewinds the write cursor to the beginning of the buffer without modifying data.
     */
    void ResetWrite();

    /**
     * @brief Moves the read cursor to an absolute position within the written data.
     * @param pos Target position in bytes from the start of the buffer.
     * @return 0 on success, -EINVAL if @p pos exceeds the write cursor.
     */
    int SeekRead(size_t pos);

    /**
     * @brief Moves the write cursor to an absolute position within the buffer capacity.
     * @param pos Target position in bytes from the start of the buffer.
     * @return 0 on success, -EINVAL if @p pos exceeds the buffer capacity.
     */
    int SeekWrite(size_t pos);

    /**
     * @brief Advances the read cursor by @p bytes without copying data.
     * @param bytes Number of bytes to skip.
     * @return 0 on success, -ENODATA if fewer than @p bytes are available to read.
     */
    int SkipRead(size_t bytes);

    /**
     * @brief Advances the write cursor by @p bytes, effectively reserving space in the buffer.
     * @param bytes Number of bytes to reserve.
     * @return 0 on success, -ENOSPC if fewer than @p bytes of free space remain.
     */
    int SkipWrite(size_t bytes);

    // -------------------------------------------------------------------------
    // Write  (return 0 on success, negative error code on failure)
    // -------------------------------------------------------------------------

    /**
     * @brief Writes a raw byte array into the stream.
     * @param data Pointer to the source data.
     * @param size Number of bytes to write.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    int Write(const uint8_t* data, size_t size);

    /**
     * @brief Writes a null-terminated C string into the stream, excluding the null terminator.
     * @param str Pointer to the source string.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    int Write(const char* str);

    /**
     * @brief Writes all written bytes of @p other into this stream.
     * @param other The source ByteStream.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    int Write(const ByteStream& other);

    /**
     * @brief Writes a boolean value as a single byte (0 or 1).
     * @param value The value to write.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    int Write(bool value);

    /**
     * @brief Writes an unsigned 8-bit integer.
     * @param value The value to write.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    int Write(uint8_t value);

    /**
     * @brief Writes an unsigned 16-bit integer respecting the configured byte order.
     * @param value The value to write.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    int Write(uint16_t value);

    /**
     * @brief Writes an unsigned 32-bit integer respecting the configured byte order.
     * @param value The value to write.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    int Write(uint32_t value);

    /**
     * @brief Writes an unsigned 64-bit integer respecting the configured byte order.
     * @param value The value to write.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    int Write(uint64_t value);

    /**
     * @brief Writes a signed 8-bit integer.
     * @param value The value to write.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    int Write(int8_t value);

    /**
     * @brief Writes a signed 16-bit integer respecting the configured byte order.
     * @param value The value to write.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    int Write(int16_t value);

    /**
     * @brief Writes a signed 32-bit integer respecting the configured byte order.
     * @param value The value to write.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    int Write(int32_t value);

    /**
     * @brief Writes a signed 64-bit integer respecting the configured byte order.
     * @param value The value to write.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    int Write(int64_t value);

    /**
     * @brief Writes a 32-bit IEEE 754 float respecting the configured byte order.
     * @param value The value to write.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    int Write(float value);

    /**
     * @brief Writes a 64-bit IEEE 754 double respecting the configured byte order.
     * @param value The value to write.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    int Write(double value);

    /**
     * @brief Writes any trivially copyable POD type using memcpy semantics (no endian conversion).
     * @tparam T A trivially copyable type.
     * @param value The value to write.
     * @return 0 on success, -ENOSPC if insufficient space.
     */
    template <typename T> int Write(const T& value);

    // -------------------------------------------------------------------------
    // Read  (return 0 on success, negative error code on failure)
    // -------------------------------------------------------------------------

    /**
     * @brief Reads @p size bytes from the stream into @p buffer.
     * @param buffer Destination buffer; must be at least @p size bytes.
     * @param size   Number of bytes to read.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Read(uint8_t* buffer, size_t size);

    /**
     * @brief Reads @p size bytes from the stream into a char buffer (no null terminator added).
     * @param buffer Destination buffer; must be at least @p size bytes.
     * @param size   Number of bytes to read.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Read(char* buffer, size_t size);

    /**
     * @brief Reads all available bytes from this stream into @p other.
     * @param other Destination ByteStream.
     * @return 0 on success, -ENOSPC if @p other has insufficient space.
     */
    int Read(ByteStream& other);

    /**
     * @brief Reads a boolean value (single byte, non-zero interpreted as true).
     * @param value Output parameter receiving the read value.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Read(bool& value);

    /**
     * @brief Reads an unsigned 8-bit integer.
     * @param value Output parameter receiving the read value.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Read(uint8_t& value);

    /**
     * @brief Reads an unsigned 16-bit integer respecting the configured byte order.
     * @param value Output parameter receiving the read value.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Read(uint16_t& value);

    /**
     * @brief Reads an unsigned 32-bit integer respecting the configured byte order.
     * @param value Output parameter receiving the read value.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Read(uint32_t& value);

    /**
     * @brief Reads an unsigned 64-bit integer respecting the configured byte order.
     * @param value Output parameter receiving the read value.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Read(uint64_t& value);

    /**
     * @brief Reads a signed 8-bit integer.
     * @param value Output parameter receiving the read value.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Read(int8_t& value);

    /**
     * @brief Reads a signed 16-bit integer respecting the configured byte order.
     * @param value Output parameter receiving the read value.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Read(int16_t& value);

    /**
     * @brief Reads a signed 32-bit integer respecting the configured byte order.
     * @param value Output parameter receiving the read value.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Read(int32_t& value);

    /**
     * @brief Reads a signed 64-bit integer respecting the configured byte order.
     * @param value Output parameter receiving the read value.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Read(int64_t& value);

    /**
     * @brief Reads a 32-bit IEEE 754 float respecting the configured byte order.
     * @param value Output parameter receiving the read value.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Read(float& value);

    /**
     * @brief Reads a 64-bit IEEE 754 double respecting the configured byte order.
     * @param value Output parameter receiving the read value.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Read(double& value);

    /**
     * @brief Reads any trivially copyable POD type using memcpy semantics (no endian conversion).
     * @tparam T A trivially copyable type.
     * @param value Output parameter receiving the read value.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    template <typename T> int Read(T& value);

    // -------------------------------------------------------------------------
    // Peek  (read without advancing the read cursor)
    // -------------------------------------------------------------------------

    /**
     * @brief Copies @p size bytes from the current read position without advancing the cursor.
     * @param buffer Destination buffer; must be at least @p size bytes.
     * @param size   Number of bytes to peek.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Peek(uint8_t* buffer, size_t size) const;

    /**
     * @brief Peeks at the next byte without advancing the read cursor.
     * @param value Output parameter receiving the peeked byte.
     * @return 0 on success, -ENODATA if no data is available.
     */
    int Peek(uint8_t& value) const;

    /**
     * @brief Peeks at the next 16-bit value respecting the configured byte order.
     * @param value Output parameter receiving the peeked value.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Peek(uint16_t& value) const;

    /**
     * @brief Peeks at the next 32-bit value respecting the configured byte order.
     * @param value Output parameter receiving the peeked value.
     * @return 0 on success, -ENODATA if insufficient data is available.
     */
    int Peek(uint32_t& value) const;

    /**
     * @brief Copies @p size bytes from an absolute buffer @p offset without moving any cursor.
     * @param offset Absolute byte offset from the start of the buffer.
     * @param buffer Destination buffer; must be at least @p size bytes.
     * @param size   Number of bytes to peek.
     * @return 0 on success, -EINVAL if @p offset is out of range, -ENODATA if insufficient data.
     */
    int PeekAt(size_t offset, uint8_t* buffer, size_t size) const;

    /**
     * @brief Reads a single byte from an absolute buffer @p offset without moving any cursor.
     * @param offset Absolute byte offset from the start of the buffer.
     * @param value  Output parameter receiving the peeked byte.
     * @return 0 on success, -EINVAL if @p offset is out of range.
     */
    int PeekAt(size_t offset, uint8_t& value) const;

    // -------------------------------------------------------------------------
    // Operators
    // -------------------------------------------------------------------------

    /**
     * @brief Stream insertion operator. Writes @p value into the stream.
     *        Errors are silently discarded; check IsWritable() beforehand if needed.
     * @tparam T The type of the value to write.
     * @param value The value to write.
     * @return Reference to this ByteStream for chaining.
     */
    template <typename T> ByteStream& operator<<(const T& value);

    /**
     * @brief Stream extraction operator. Reads a value of type @p T from the stream.
     *        Errors are silently discarded; check IsReadable() beforehand if needed.
     * @tparam T The type of the value to read.
     * @param value Output parameter receiving the read value.
     * @return Reference to this ByteStream for chaining.
     */
    template <typename T> ByteStream& operator>>(T& value);

    /**
     * @brief Read-only index operator. Accesses a byte at @p index from the buffer start.
     *        No cursor movement. Behaviour is undefined if @p index >= GetCapacity().
     * @param index Zero-based byte offset into the buffer.
     * @return The byte value at @p index.
     */
    uint8_t operator[](size_t index) const;

    /**
     * @brief Read-write index operator. Accesses a byte at @p index from the buffer start.
     *        No cursor movement. Behaviour is undefined if @p index >= GetCapacity().
     * @param index Zero-based byte offset into the buffer.
     * @return Reference to the byte at @p index.
     */
    uint8_t& operator[](size_t index);

    /**
     * @brief Equality operator. Compares the written content of both streams byte-by-byte.
     * @param other The ByteStream to compare against.
     * @return true if both streams have identical written content.
     */
    bool operator==(const ByteStream& other) const;

    /**
     * @brief Inequality operator. Returns the logical inverse of operator==.
     * @param other The ByteStream to compare against.
     * @return true if the written content differs.
     */
    bool operator!=(const ByteStream& other) const;

    /**
     * @brief Contextual bool conversion. Evaluates to true if IsReadable().
     */
    explicit operator bool() const;

    private:
    uint8_t* m_data;
    size_t m_capacity;
    size_t m_writePos;
    size_t m_readPos;
    bool m_ownsData;
    Endian m_endian;
};

// -----------------------------------------------------------------------------
// Inline template implementations
// -----------------------------------------------------------------------------

/**
 * @brief Writes any trivially copyable POD type using memcpy semantics (no endian conversion).
 * @tparam T A trivially copyable type.
 * @param value The value to write.
 * @return 0 on success, -ENOSPC if insufficient space.
 */
template <typename T> int ByteStream::Write(const T& value)
{
    return Write(reinterpret_cast<const uint8_t*>(&value), sizeof(T));
}

/**
 * @brief Reads any trivially copyable POD type using memcpy semantics (no endian conversion).
 * @tparam T A trivially copyable type.
 * @param value Output parameter receiving the read value.
 * @return 0 on success, -ENODATA if insufficient data is available.
 */
template <typename T> int ByteStream::Read(T& value)
{
    return Read(reinterpret_cast<uint8_t*>(&value), sizeof(T));
}

/**
 * @brief Stream insertion operator. Writes @p value into the stream.
 * @tparam T The type of the value to write.
 * @param value The value to write.
 * @return Reference to this ByteStream for chaining.
 */
template <typename T> ByteStream& ByteStream::operator<<(const T& value)
{
    Write(value);
    return *this;
}

/**
 * @brief Stream extraction operator. Reads a value of type @p T from the stream.
 * @tparam T The type of the value to read.
 * @param value Output parameter receiving the read value.
 * @return Reference to this ByteStream for chaining.
 */
template <typename T> ByteStream& ByteStream::operator>>(T& value)
{
    Read(value);
    return *this;
}

} // namespace Kite