#include "input/TLoraPagerKeyPolicy.h"

#include <cstddef>

namespace
{
constexpr uint8_t kSymKey = 21;
constexpr uint8_t kShiftKey = 29;
constexpr uint8_t kSpaceKey = 31;
constexpr uint8_t kBackspaceKey = 30;
constexpr uint8_t kEnterKey = 20;

constexpr uint8_t keyMap[31][3] = {
    {'q', 'Q', '1'}, {'w', 'W', '2'},    {'e', 'E', '3'},  {'r', 'R', '4'}, {'t', 'T', '5'}, {'y', 'Y', '6'},    {'u', 'U', '7'},
    {'i', 'I', '8'}, {'o', 'O', '9'},    {'p', 'P', '0'},  {'a', 'A', '*'}, {'s', 'S', '/'}, {'d', 'D', '+'},    {'f', 'F', '-'},
    {'g', 'G', '='}, {'h', 'H', ':'},    {'j', 'J', '\''}, {'k', 'K', '"'}, {'l', 'L', '@'}, {0x0d, 0x09, 0x0d}, {0, 0, 0},
    {'z', 'Z', '_'}, {'x', 'X', '$'},    {'c', 'C', ';'},  {'v', 'V', '?'}, {'b', 'B', '!'}, {'n', 'N', ','},    {'m', 'M', '.'},
    {0, 0, 0},       {0x08, 0x08, 0x1b}, {' ', ' ', ' '}};
} // namespace

TLoraPagerKeyPolicy::Result TLoraPagerKeyPolicy::makeResult(uint8_t rawKeyCode, bool pressed, const ActiveAction &active) const
{
    Result result{};
    result.emit = active.kind != ActionKind::None && active.kind != ActionKind::Consumed;
    result.consume = active.kind != ActionKind::None;
    result.action = {active.kind, active.codepoint, rawKeyCode, pressed, active.appliedModifier, active.encoderDelta};
    result.latchedModifier = latchedModifier;
    result.symHeld = symPhysicalDown;
    return result;
}

void TLoraPagerKeyPolicy::expireModifier(uint32_t timestampMs)
{
    if (latchedModifier != Modifier::None && timestampMs - modifierSinceMs >= modifierTimeoutMs) {
        latchedModifier = Modifier::None;
    }
}

TLoraPagerKeyPolicy::Result TLoraPagerKeyPolicy::feed(const RawEvent &event, const Context &context)
{
    if (event.keyCode == 0 || event.keyCode > 31) {
        return Result{false, false, {}, latchedModifier, symPhysicalDown};
    }

    const size_t index = event.keyCode - 1;
    if (!event.pressed) {
        const ActiveAction active = activeActions[index];
        activeActions[index] = {};

        // A wake-only modifier is paired like any other semantic action and
        // must not arm a modifier when the display becomes active.
        if (active.kind != ActionKind::None) {
            if (event.keyCode == kShiftKey) {
                shiftPhysicalDown = false;
            } else if (event.keyCode == kSymKey) {
                symPhysicalDown = false;
                symUsedForScroll = false;
            }
            return makeResult(event.keyCode, false, active);
        }

        if (event.keyCode == kShiftKey) {
            if (!shiftPhysicalDown) {
                return Result{false, false, {}, latchedModifier, symPhysicalDown};
            }
            shiftPhysicalDown = false;
            return Result{false, true, {}, latchedModifier, symPhysicalDown};
        }

        if (event.keyCode == kSymKey) {
            if (!symPhysicalDown) {
                return Result{false, false, {}, latchedModifier, symPhysicalDown};
            }
            const bool tap = !symUsedForScroll && event.timestampMs - symPressedAtMs < symHoldThresholdMs;
            symPhysicalDown = false;
            symUsedForScroll = false;
            if (tap) {
                latchedModifier = latchedModifier == Modifier::Sym ? Modifier::None : Modifier::Sym;
                modifierSinceMs = event.timestampMs;
            } else {
                latchedModifier = Modifier::None;
            }
            return Result{false, true, {}, latchedModifier, symPhysicalDown};
        }
        return Result{false, false, {}, latchedModifier, symPhysicalDown};
    }

    // Duplicate presses cannot produce duplicate text or commands.
    if (activeActions[index].kind != ActionKind::None) {
        return Result{false, true, {}, latchedModifier, symPhysicalDown};
    }

    expireModifier(event.timestampMs);
    if (!context.displayAwake) {
        const ActiveAction wake{ActionKind::Wake, 0, Modifier::None};
        activeActions[index] = wake;
        return makeResult(event.keyCode, true, wake);
    }

    if (event.keyCode == kShiftKey) {
        if (shiftPhysicalDown) {
            return Result{false, true, {}, latchedModifier, symPhysicalDown};
        }
        shiftPhysicalDown = true;
        latchedModifier = latchedModifier == Modifier::Shift ? Modifier::None : Modifier::Shift;
        modifierSinceMs = event.timestampMs;
        return Result{false, true, {}, latchedModifier, symPhysicalDown};
    }

    if (event.keyCode == kSymKey) {
        if (symPhysicalDown) {
            return Result{false, true, {}, latchedModifier, symPhysicalDown};
        }
        symPhysicalDown = true;
        symUsedForScroll = false;
        symPressedAtMs = event.timestampMs;
        // Sym is armed only on a short release, so a hold can be scroll-only.
        return Result{false, true, {}, latchedModifier, symPhysicalDown};
    }

    const Modifier appliedModifier = latchedModifier;
    ActiveAction active{};

    if (event.keyCode == kSpaceKey && appliedModifier == Modifier::Shift) {
        active.kind = context.textInput ? ActionKind::ToggleInputMode : ActionKind::Consumed;
        active.appliedModifier = appliedModifier;
    } else if (event.keyCode == kBackspaceKey) {
        active.kind = context.textInput ? ActionKind::TextBackspace : ActionKind::NavigateBack;
        active.codepoint = 0x08;
        active.appliedModifier = appliedModifier;
    } else if (event.keyCode == kEnterKey) {
        if (appliedModifier == Modifier::Sym) {
            active.kind = ActionKind::NavigateNext;
            active.codepoint = 0x09;
        } else {
            active.kind = context.textInput ? ActionKind::Submit : ActionKind::Activate;
            active.codepoint = 0x0d;
        }
        active.appliedModifier = appliedModifier;
    } else if (event.keyCode == kSpaceKey && appliedModifier == Modifier::Sym) {
        active.kind = ActionKind::ToggleKeyboardBacklight;
        active.appliedModifier = appliedModifier;
    } else {
        const uint8_t key = keyMap[index][static_cast<uint8_t>(appliedModifier)];
        if (key != 0) {
            active = {ActionKind::Character, key, appliedModifier};
        }
    }

    // A non-modifier consumes the current one-shot state even when the action
    // is intentionally suppressed outside text input.
    latchedModifier = Modifier::None;
    if (active.kind == ActionKind::None) {
        return Result{false, false, {}, latchedModifier, symPhysicalDown};
    }
    activeActions[index] = active;
    return makeResult(event.keyCode, true, active);
}

TLoraPagerKeyPolicy::Result TLoraPagerKeyPolicy::routeEncoder(int32_t delta, uint32_t timestampMs, const Context &context)
{
    expireModifier(timestampMs);
    if (delta == 0) {
        return Result{false, false, {}, latchedModifier, symPhysicalDown};
    }

    ActiveAction active{};
    if (!context.displayAwake) {
        active.kind = ActionKind::Wake;
    } else if (symPhysicalDown) {
        symUsedForScroll = true;
        active.kind = delta < 0 ? ActionKind::ScrollUp : ActionKind::ScrollDown;
        active.appliedModifier = Modifier::Sym;
        active.encoderDelta = delta;
    } else {
        return Result{false, false, {}, latchedModifier, symPhysicalDown};
    }
    return makeResult(0, true, active);
}

void TLoraPagerKeyPolicy::reset()
{
    latchedModifier = Modifier::None;
    modifierSinceMs = 0;
    symPressedAtMs = 0;
    shiftPhysicalDown = false;
    symPhysicalDown = false;
    symUsedForScroll = false;
    for (auto &active : activeActions) {
        active = {};
    }
}
