#include "util/LogMessage.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <doctest/doctest.h>

namespace
{
std::vector<uint8_t> serialize(const LogMessageEnv &message)
{
    std::vector<uint8_t> data;
    message.serialize([&data](const uint8_t *bytes, size_t length) {
        data.insert(data.end(), bytes, bytes + length);
        return length;
    });
    return data;
}

size_t deserialize(LogMessageEnv &message, const std::vector<uint8_t> &data)
{
    size_t offset = 0;
    return message.deserialize([&](uint8_t *bytes, size_t length) {
        const size_t available = std::min(length, data.size() - std::min(offset, data.size()));
        if (available > 0)
            std::memcpy(bytes, data.data() + offset, available);
        offset += available;
        return available;
    });
}

void checkRoundTrip(size_t length)
{
    const std::string payload(length, 'x');
    const LogMessageEnv original(1, 2, 3, 4, LogMessage::eDefault, false, length,
                                 reinterpret_cast<const uint8_t *>(payload.data()));
    const auto data = serialize(original);
    CHECK(data.size() == original.size());

    LogMessageEnv restored;
    CHECK(deserialize(restored, data) == data.size());
    CHECK(restored.length() == length);
    CHECK(restored.bytes[length] == 0);
    CHECK(std::memcmp(restored.bytes, payload.data(), length) == 0);
}
} // namespace

TEST_CASE("message log preserves every valid wire payload boundary")
{
    checkRoundTrip(0);
    checkRoundTrip(232);
    checkRoundTrip(233);
}

TEST_CASE("message log rejects oversized and truncated records")
{
    const std::string payload(233, 'x');
    const LogMessageEnv original(1, 2, 3, 4, LogMessage::eDefault, false, payload.size(),
                                 reinterpret_cast<const uint8_t *>(payload.data()));

    auto oversized = serialize(original);
    const uint16_t invalidSize = 234;
    std::memcpy(oversized.data(), &invalidSize, sizeof(invalidSize));
    LogMessageEnv invalid;
    CHECK(deserialize(invalid, oversized) == 0);
    CHECK(invalid.length() == 0);
    CHECK(invalid.bytes[0] == 0);

    auto truncated = serialize(original);
    truncated.pop_back();
    LogMessageEnv incomplete;
    CHECK(deserialize(incomplete, truncated) == 0);
    CHECK(incomplete.length() == 0);
    CHECK(incomplete.bytes[0] == 0);
}
