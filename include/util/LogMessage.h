#pragma once

#include "ILogEntry.h"
#include <assert.h>
#include <ctime>
#include <memory.h>

constexpr uint32_t messagePayloadSize = 233;

/**
 * @brief Header for storing message logs containing the actual size of the payload
 * Note: this struct does have vtable pointers, i.e. sizeof(LogMessageHeader)-8 is the real data size
 */
struct LogMessageHeader : public ILogEntry {
    size_t length(void) const override { return _size; }

    uint16_t _size;
    time_t time;
    uint32_t from;
    uint32_t to;
    uint8_t ch;
    enum MsgStatus : uint8_t { eNone, eDefault, eHeard, eNoResponse, eAcked, eFailed, eDeleted, eUnread } status;
    bool trashFlag;
    uint32_t reserved;
};

/**
 * @brief Structure for storing message logs containing the actual payload
 */
struct LogMessage : public LogMessageHeader {
    // The serialized payload is still at most 233 bytes. The extra byte is an
    // in-memory terminator for display APIs that consume C strings.
    uint8_t bytes[messagePayloadSize + 1];
};

/**
 * Log message envelope that implements the actual interface for ILogEntry
 * (size, serialize and deserialize)
 */
class LogMessageEnv : public LogMessage
{
  public:
    LogMessageEnv(void) = default;
    LogMessageEnv(uint32_t _from, uint32_t _to, uint16_t _ch, time_t _time, MsgStatus _status, bool _trashFlag, uint32_t _len,
                  const uint8_t *msg)
    {
        assert(_len <= messagePayloadSize);
        assert(_len == 0 || msg != nullptr);
        _size = (uint16_t)_len;
        time = _time;
        from = _from;
        to = _to;
        ch = _ch;
        status = _status;
        trashFlag = _trashFlag;
        reserved = 0;
        if (_len > 0)
            memcpy(bytes, msg, _len);
        bytes[_len] = 0;
    }

    size_t size(void) const override { return sizeof(LogMessageHeader) - 8 + _size; }

    virtual size_t serialize(std::function<size_t(const uint8_t *, size_t)> write) const override
    {
        return write((uint8_t *)&_size, sizeof(LogMessageHeader) - 8) + write(bytes, _size);
    }

    virtual size_t deserialize(std::function<size_t(uint8_t *, size_t)> read) override
    {
        constexpr size_t headerSize = sizeof(LogMessageHeader) - 8;
        const size_t headerRead = read((uint8_t *)&_size, headerSize);
        if (headerRead != headerSize || _size > messagePayloadSize) {
            _size = 0;
            bytes[0] = 0;
            return 0;
        }

        const size_t payloadRead = _size == 0 ? 0 : read(bytes, _size);
        if (payloadRead != _size) {
            _size = 0;
            bytes[0] = 0;
            return 0;
        }

        bytes[_size] = 0;
        return headerRead + payloadRead;
    }
};
