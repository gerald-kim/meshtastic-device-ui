#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

class MessageTextController
{
  public:
    // Preserve the existing Messages composer limit as a UTF-8 byte budget.
    // The receive/log path separately supports the protocol's 233-byte payload.
    static constexpr size_t MAX_PAYLOAD_BYTES = 220;
    static constexpr size_t MAX_CODEPOINT_BYTES = 4;

    struct ViewState {
        std::array<char, MAX_PAYLOAD_BYTES + 1> text{};
        size_t cursor = 0;
        size_t preeditStart = 0;
        size_t preeditEnd = 0;
        bool hasPreedit = false;
    };

    enum class Result : uint8_t {
        Applied,
        NoChange,
        InvalidUtf8,
        PayloadFull,
        ActivePreedit,
    };

    MessageTextController();

    Result applyComposition(const char *committed, const char *preedit);
    Result replacePreedit(const char *preedit);
    Result commitPreeditAndInsert(const char *text);
    Result commitPreedit();
    Result backspaceCommitted();
    Result moveCursorLeft();
    Result moveCursorRight();
    Result setCursorCodepoint(size_t position);

    void reset();
    bool render(char (&out)[MAX_PAYLOAD_BYTES + 1]) const;
    bool render(ViewState &out) const;

    const char *committedText() const { return committed.data(); }
    const char *preeditText() const { return preedit.data(); }
    size_t committedSize() const { return committedBytes; }
    size_t preeditSize() const { return preeditBytes; }
    size_t payloadSize() const { return committedBytes + preeditBytes; }
    size_t cursorBytePosition() const { return cursorByte; }
    size_t cursorCodepointPosition() const;
    bool hasPreedit() const { return preeditBytes != 0; }

    static bool isValidUtf8(const char *text);

  private:
    static bool decodeNext(const char *text, size_t length, size_t &offset, uint32_t &codepoint);
    static size_t previousCodepoint(const char *text, size_t offset);
    static size_t codepointCount(const char *text, size_t length);
    static size_t bytePositionForCodepoint(const char *text, size_t length, size_t position);

    Result insertAtCursor(const char *text, size_t length);
    void storePreedit(const char *text, size_t length);

    std::array<char, MAX_PAYLOAD_BYTES + 1> committed{};
    std::array<char, MAX_CODEPOINT_BYTES + 1> preedit{};
    size_t committedBytes = 0;
    size_t preeditBytes = 0;
    size_t cursorByte = 0;
};
