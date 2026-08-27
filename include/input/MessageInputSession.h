#pragma once

#include "input/HangulComposer.h"
#include "input/MessageTextController.h"

#include <cstddef>
#include <cstdint>

/**
 * Product owner for the Messages draft, Korean composition, and input mode.
 *
 * Mutations are transactional: the composer and text controller advance
 * together only when the resulting UTF-8 payload fits the 220-byte outbound
 * budget. LVGL is a rendered view of this object, never the text owner.
 */
class MessageInputSession
{
  public:
    enum class Mode : uint8_t { English, Korean };
    enum class Result : uint8_t { Applied, NoChange, PayloadFull, InvalidUtf8 };

    Result insertAscii(uint8_t ascii);
    Result insertUtf8(const char *text, size_t length);
    Result replaceUtf8(const char *text, size_t length);
    Result backspace();
    Result moveCursorLeft();
    Result moveCursorRight();
    Result setCursorCodepoint(size_t position);
    Result toggleMode();
    Result flush();
    Result prepareSubmit();

    void clearAfterSubmit();
    void reset();

    Mode mode() const { return inputMode; }
    bool isKoreanMode() const { return inputMode == Mode::Korean; }
    const MessageTextController &text() const { return controller; }
    MessageTextController::ViewState viewState() const;

  private:
    static Result translate(MessageTextController::Result result);
    Result applyCandidate(const HangulComposer &candidate, const HangulComposer::Update &update);

    HangulComposer composer;
    MessageTextController controller;
    Mode inputMode = Mode::English;
};
