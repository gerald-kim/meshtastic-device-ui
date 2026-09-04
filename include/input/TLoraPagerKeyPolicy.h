#pragma once

#include <cstdint>

/**
 * Heap-free owner of T-LoRa Pager raw-key modifier and context policy.
 *
 * Hardware and native replay are raw producers. Both must feed this class;
 * neither is allowed to interpret Shift, Sym, wake, text, or navigation.
 */
class TLoraPagerKeyPolicy
{
  public:
    static constexpr uint32_t modifierTimeoutMs = 1500;
    static constexpr uint32_t symHoldThresholdMs = 700;

    enum class Modifier : uint8_t { None, Shift, Sym };

    enum class ActionKind : uint8_t {
        None,
        Consumed,
        Character,
        TextBackspace,
        NavigateBack,
        Submit,
        Activate,
        NavigateNext,
        ToggleInputMode,
        ToggleKeyboardBacklight,
        ScrollUp,
        ScrollDown,
        Wake,
    };

    struct RawEvent {
        uint8_t keyCode = 0;
        bool pressed = false;
        uint32_t timestampMs = 0;
    };

    struct Context {
        bool displayAwake = true;
        bool textInput = false;
    };

    struct Action {
        ActionKind kind = ActionKind::None;
        uint32_t codepoint = 0;
        uint8_t rawKeyCode = 0;
        bool pressed = false;
        Modifier appliedModifier = Modifier::None;
        int32_t encoderDelta = 0;
    };

    struct Result {
        bool emit = false;
        bool consume = false;
        Action action{};
        Modifier latchedModifier = Modifier::None;
        bool symHeld = false;
    };

    Result feed(const RawEvent &event) { return feed(event, Context{}); }
    Result feed(const RawEvent &event, const Context &context);
    Result routeEncoder(int32_t delta, uint32_t timestampMs) { return routeEncoder(delta, timestampMs, Context{}); }
    Result routeEncoder(int32_t delta, uint32_t timestampMs, const Context &context);

    Modifier modifier() const { return latchedModifier; }
    bool isSymHeld() const { return symPhysicalDown; }
    void reset();

  private:
    struct ActiveAction {
        ActionKind kind = ActionKind::None;
        uint8_t codepoint = 0;
        Modifier appliedModifier = Modifier::None;
        int32_t encoderDelta = 0;
    };

    Result makeResult(uint8_t rawKeyCode, bool pressed, const ActiveAction &active) const;
    void expireModifier(uint32_t timestampMs);

    Modifier latchedModifier = Modifier::None;
    uint32_t modifierSinceMs = 0;
    uint32_t symPressedAtMs = 0;
    bool shiftPhysicalDown = false;
    bool symPhysicalDown = false;
    bool symUsedForScroll = false;
    ActiveAction activeActions[31]{};
};
