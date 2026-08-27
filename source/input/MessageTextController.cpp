#include "input/MessageTextController.h"

#include <cstring>

MessageTextController::MessageTextController()
{
    reset();
}

void MessageTextController::reset()
{
    committed.fill('\0');
    preedit.fill('\0');
    committedBytes = 0;
    preeditBytes = 0;
    cursorByte = 0;
}

bool MessageTextController::decodeNext(const char *text, size_t length, size_t &offset, uint32_t &codepoint)
{
    if (offset >= length)
        return false;

    const auto first = static_cast<uint8_t>(text[offset]);
    size_t count = 0;
    uint32_t minimum = 0;
    if (first <= 0x7f) {
        count = 1;
        codepoint = first;
    } else if ((first & 0xe0) == 0xc0) {
        count = 2;
        codepoint = first & 0x1f;
        minimum = 0x80;
    } else if ((first & 0xf0) == 0xe0) {
        count = 3;
        codepoint = first & 0x0f;
        minimum = 0x800;
    } else if ((first & 0xf8) == 0xf0) {
        count = 4;
        codepoint = first & 0x07;
        minimum = 0x10000;
    } else {
        return false;
    }

    if (offset + count > length)
        return false;

    for (size_t i = 1; i < count; ++i) {
        const auto next = static_cast<uint8_t>(text[offset + i]);
        if ((next & 0xc0) != 0x80)
            return false;
        codepoint = (codepoint << 6) | (next & 0x3f);
    }

    if ((count > 1 && codepoint < minimum) || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff))
        return false;

    offset += count;
    return true;
}

bool MessageTextController::isValidUtf8(const char *text)
{
    if (!text)
        return false;

    const size_t length = std::strlen(text);
    size_t offset = 0;
    uint32_t codepoint = 0;
    while (offset < length) {
        if (!decodeNext(text, length, offset, codepoint))
            return false;
    }
    return true;
}

size_t MessageTextController::codepointCount(const char *text, size_t length)
{
    size_t offset = 0;
    size_t count = 0;
    uint32_t codepoint = 0;
    while (offset < length) {
        if (!decodeNext(text, length, offset, codepoint))
            return static_cast<size_t>(-1);
        ++count;
    }
    return count;
}

size_t MessageTextController::previousCodepoint(const char *text, size_t offset)
{
    if (offset == 0)
        return 0;

    --offset;
    while (offset > 0 && (static_cast<uint8_t>(text[offset]) & 0xc0) == 0x80)
        --offset;
    return offset;
}

size_t MessageTextController::bytePositionForCodepoint(const char *text, size_t length, size_t position)
{
    size_t offset = 0;
    size_t count = 0;
    uint32_t codepoint = 0;
    while (offset < length && count < position) {
        if (!decodeNext(text, length, offset, codepoint))
            return static_cast<size_t>(-1);
        ++count;
    }
    return count == position ? offset : static_cast<size_t>(-1);
}

MessageTextController::Result MessageTextController::insertAtCursor(const char *text, size_t length)
{
    if (length == 0)
        return Result::NoChange;
    if (committedBytes + length > MAX_PAYLOAD_BYTES)
        return Result::PayloadFull;

    std::memmove(committed.data() + cursorByte + length, committed.data() + cursorByte, committedBytes - cursorByte + 1);
    std::memcpy(committed.data() + cursorByte, text, length);
    committedBytes += length;
    cursorByte += length;
    return Result::Applied;
}

void MessageTextController::storePreedit(const char *text, size_t length)
{
    preedit.fill('\0');
    if (length)
        std::memcpy(preedit.data(), text, length);
    preeditBytes = length;
}

MessageTextController::Result MessageTextController::applyComposition(const char *newCommitted, const char *newPreedit)
{
    if (!newCommitted || !newPreedit || !isValidUtf8(newCommitted) || !isValidUtf8(newPreedit))
        return Result::InvalidUtf8;

    const size_t commitLength = std::strlen(newCommitted);
    const size_t nextPreeditLength = std::strlen(newPreedit);
    if (nextPreeditLength > MAX_CODEPOINT_BYTES || codepointCount(newPreedit, nextPreeditLength) > 1)
        return Result::InvalidUtf8;
    if (committedBytes + commitLength + nextPreeditLength > MAX_PAYLOAD_BYTES)
        return Result::PayloadFull;

    const bool preeditChanged =
        preeditBytes != nextPreeditLength || std::memcmp(preedit.data(), newPreedit, nextPreeditLength) != 0;
    const Result inserted = insertAtCursor(newCommitted, commitLength);
    storePreedit(newPreedit, nextPreeditLength);
    return inserted == Result::NoChange && !preeditChanged ? Result::NoChange : Result::Applied;
}

MessageTextController::Result MessageTextController::replacePreedit(const char *newPreedit)
{
    return applyComposition("", newPreedit);
}

MessageTextController::Result MessageTextController::commitPreedit()
{
    if (!preeditBytes)
        return Result::NoChange;

    const Result result = insertAtCursor(preedit.data(), preeditBytes);
    if (result == Result::Applied)
        storePreedit("", 0);
    return result;
}

MessageTextController::Result MessageTextController::commitPreeditAndInsert(const char *text)
{
    if (!text || !isValidUtf8(text))
        return Result::InvalidUtf8;

    const size_t length = std::strlen(text);
    if (committedBytes + preeditBytes + length > MAX_PAYLOAD_BYTES)
        return Result::PayloadFull;

    Result committedResult = Result::NoChange;
    if (preeditBytes) {
        committedResult = commitPreedit();
        if (committedResult != Result::Applied)
            return committedResult;
    }
    const Result inserted = insertAtCursor(text, length);
    return inserted == Result::NoChange ? committedResult : inserted;
}

MessageTextController::Result MessageTextController::backspaceCommitted()
{
    if (preeditBytes)
        return Result::ActivePreedit;
    if (cursorByte == 0)
        return Result::NoChange;

    const size_t previous = previousCodepoint(committed.data(), cursorByte);
    const size_t removed = cursorByte - previous;
    std::memmove(committed.data() + previous, committed.data() + cursorByte, committedBytes - cursorByte + 1);
    committedBytes -= removed;
    cursorByte = previous;
    return Result::Applied;
}

MessageTextController::Result MessageTextController::moveCursorLeft()
{
    if (preeditBytes)
        commitPreedit();
    if (cursorByte == 0)
        return Result::NoChange;
    cursorByte = previousCodepoint(committed.data(), cursorByte);
    return Result::Applied;
}

MessageTextController::Result MessageTextController::moveCursorRight()
{
    if (preeditBytes)
        commitPreedit();
    if (cursorByte == committedBytes)
        return Result::NoChange;

    size_t next = cursorByte;
    uint32_t codepoint = 0;
    if (!decodeNext(committed.data(), committedBytes, next, codepoint))
        return Result::InvalidUtf8;
    cursorByte = next;
    return Result::Applied;
}

MessageTextController::Result MessageTextController::setCursorCodepoint(size_t position)
{
    if (preeditBytes)
        commitPreedit();
    const size_t bytePosition = bytePositionForCodepoint(committed.data(), committedBytes, position);
    if (bytePosition == static_cast<size_t>(-1))
        return Result::InvalidUtf8;
    if (bytePosition == cursorByte)
        return Result::NoChange;
    cursorByte = bytePosition;
    return Result::Applied;
}

size_t MessageTextController::cursorCodepointPosition() const
{
    return codepointCount(committed.data(), cursorByte);
}

bool MessageTextController::render(char (&out)[MAX_PAYLOAD_BYTES + 1]) const
{
    if (payloadSize() > MAX_PAYLOAD_BYTES)
        return false;

    std::memcpy(out, committed.data(), cursorByte);
    std::memcpy(out + cursorByte, preedit.data(), preeditBytes);
    std::memcpy(out + cursorByte + preeditBytes, committed.data() + cursorByte, committedBytes - cursorByte);
    out[payloadSize()] = '\0';
    return true;
}

bool MessageTextController::render(ViewState &out) const
{
    if (payloadSize() > MAX_PAYLOAD_BYTES)
        return false;

    std::memcpy(out.text.data(), committed.data(), cursorByte);
    std::memcpy(out.text.data() + cursorByte, preedit.data(), preeditBytes);
    std::memcpy(out.text.data() + cursorByte + preeditBytes, committed.data() + cursorByte, committedBytes - cursorByte);
    out.text[payloadSize()] = '\0';

    out.preeditStart = cursorCodepointPosition();
    out.hasPreedit = preeditBytes != 0;
    out.preeditEnd = out.preeditStart + (out.hasPreedit ? 1 : 0);
    out.cursor = out.preeditEnd;
    return true;
}
