#include "input/MessageInputSession.h"

#include <array>
#include <cstring>

MessageInputSession::Result MessageInputSession::translate(MessageTextController::Result result)
{
    switch (result) {
    case MessageTextController::Result::Applied:
        return Result::Applied;
    case MessageTextController::Result::PayloadFull:
        return Result::PayloadFull;
    case MessageTextController::Result::InvalidUtf8:
        return Result::InvalidUtf8;
    case MessageTextController::Result::NoChange:
    case MessageTextController::Result::ActivePreedit:
        return Result::NoChange;
    }
    return Result::InvalidUtf8;
}

MessageInputSession::Result MessageInputSession::applyCandidate(const HangulComposer &candidate,
                                                                const HangulComposer::Update &update)
{
    std::array<char, 9> committed{};
    std::array<char, MessageTextController::MAX_CODEPOINT_BYTES + 1> preedit{};
    std::memcpy(committed.data(), update.committed().data(), update.committed().size());
    std::memcpy(preedit.data(), update.preedit().data(), update.preedit().size());

    const auto result = controller.applyComposition(committed.data(), preedit.data());
    if (result == MessageTextController::Result::Applied || result == MessageTextController::Result::NoChange)
        composer = candidate;
    return translate(result);
}

MessageInputSession::Result MessageInputSession::insertAscii(uint8_t ascii)
{
    if (ascii < 0x20 || ascii > 0x7e)
        return Result::NoChange;

    if (!isKoreanMode()) {
        const char text[]{static_cast<char>(ascii), '\0'};
        return translate(controller.commitPreeditAndInsert(text));
    }

    HangulComposer candidate = composer;
    const auto update = candidate.feed(ascii);
    if (!update.consumed)
        return Result::NoChange;
    return applyCandidate(candidate, update);
}

MessageInputSession::Result MessageInputSession::insertUtf8(const char *text, size_t length)
{
    if (!text || length > MessageTextController::MAX_PAYLOAD_BYTES)
        return length > MessageTextController::MAX_PAYLOAD_BYTES ? Result::PayloadFull : Result::InvalidUtf8;

    std::array<char, MessageTextController::MAX_PAYLOAD_BYTES + 1> terminated{};
    std::memcpy(terminated.data(), text, length);
    if (std::strlen(terminated.data()) != length || !MessageTextController::isValidUtf8(terminated.data()))
        return Result::InvalidUtf8;

    MessageInputSession candidate = *this;
    const Result flushed = candidate.flush();
    if (flushed == Result::PayloadFull || flushed == Result::InvalidUtf8)
        return flushed;
    const auto inserted = candidate.controller.commitPreeditAndInsert(terminated.data());
    if (inserted == MessageTextController::Result::Applied || inserted == MessageTextController::Result::NoChange)
        *this = candidate;
    return translate(inserted);
}

MessageInputSession::Result MessageInputSession::replaceUtf8(const char *text, size_t length)
{
    if (!text || length > MessageTextController::MAX_PAYLOAD_BYTES)
        return length > MessageTextController::MAX_PAYLOAD_BYTES ? Result::PayloadFull : Result::InvalidUtf8;

    std::array<char, MessageTextController::MAX_PAYLOAD_BYTES + 1> terminated{};
    std::memcpy(terminated.data(), text, length);
    if (std::strlen(terminated.data()) != length || !MessageTextController::isValidUtf8(terminated.data()))
        return Result::InvalidUtf8;

    MessageTextController candidate;
    const auto result = candidate.commitPreeditAndInsert(terminated.data());
    if (result == MessageTextController::Result::PayloadFull)
        return Result::PayloadFull;
    if (result == MessageTextController::Result::InvalidUtf8)
        return Result::InvalidUtf8;

    const auto previous = viewState();
    composer.reset();
    controller = candidate;
    return std::strcmp(previous.text.data(), terminated.data()) == 0 && !previous.hasPreedit ? Result::NoChange : Result::Applied;
}

MessageInputSession::Result MessageInputSession::backspace()
{
    HangulComposer candidate = composer;
    const auto update = candidate.backspace();
    if (update.consumed)
        return applyCandidate(candidate, update);
    return translate(controller.backspaceCommitted());
}

MessageInputSession::Result MessageInputSession::moveCursorLeft()
{
    const Result flushed = flush();
    if (flushed == Result::PayloadFull || flushed == Result::InvalidUtf8)
        return flushed;
    return translate(controller.moveCursorLeft());
}

MessageInputSession::Result MessageInputSession::moveCursorRight()
{
    const Result flushed = flush();
    if (flushed == Result::PayloadFull || flushed == Result::InvalidUtf8)
        return flushed;
    return translate(controller.moveCursorRight());
}

MessageInputSession::Result MessageInputSession::setCursorCodepoint(size_t position)
{
    const Result flushed = flush();
    if (flushed == Result::PayloadFull || flushed == Result::InvalidUtf8)
        return flushed;
    return translate(controller.setCursorCodepoint(position));
}

MessageInputSession::Result MessageInputSession::toggleMode()
{
    const Result flushed = flush();
    if (flushed == Result::PayloadFull || flushed == Result::InvalidUtf8)
        return flushed;
    inputMode = isKoreanMode() ? Mode::English : Mode::Korean;
    return Result::Applied;
}

MessageInputSession::Result MessageInputSession::flush()
{
    HangulComposer candidate = composer;
    return applyCandidate(candidate, candidate.flush());
}

MessageInputSession::Result MessageInputSession::prepareSubmit()
{
    return flush();
}

void MessageInputSession::clearAfterSubmit()
{
    composer.reset();
    controller.reset();
}

void MessageInputSession::reset()
{
    clearAfterSubmit();
    inputMode = Mode::English;
}

MessageTextController::ViewState MessageInputSession::viewState() const
{
    MessageTextController::ViewState state;
    controller.render(state);
    return state;
}
