#include "input/TLoraPagerInputController.h"

#include <cstddef>

bool TLoraPagerInputController::isTextAction(TLoraPagerKeyPolicy::ActionKind kind)
{
    using Kind = TLoraPagerKeyPolicy::ActionKind;
    return kind == Kind::Character || kind == Kind::TextBackspace || kind == Kind::Submit || kind == Kind::ToggleInputMode;
}

bool TLoraPagerInputController::dispatchPress(const TLoraPagerKeyPolicy::Result &result,
                                              const TLoraPagerKeyPolicy::Context &context)
{
    using Kind = TLoraPagerKeyPolicy::ActionKind;
    if (!result.emit || !result.action.pressed)
        return result.consume;

    const bool sinkConsumed = sink && sink->handleTLoraPagerAction(result.action);
    const Kind kind = result.action.kind;
    const bool controllerOwned = kind == Kind::Wake || kind == Kind::ToggleKeyboardBacklight || kind == Kind::Consumed ||
                                 (context.textInput && isTextAction(kind));
    return result.consume && (sinkConsumed || controllerOwned);
}

TLoraPagerInputController::Dispatch TLoraPagerInputController::feed(const TLoraPagerKeyPolicy::RawEvent &event,
                                                                    const TLoraPagerKeyPolicy::Context &context)
{
    Dispatch dispatch{policy.feed(event, context), false};
    if (event.keyCode == 0 || event.keyCode > 31)
        return dispatch;

    const size_t index = event.keyCode - 1;
    if (event.pressed) {
        if (keyDown[index]) {
            dispatch.consumed = true;
            return dispatch;
        }
        keyDown[index] = true;
        dispatch.consumed = dispatchPress(dispatch.policy, context);
        consumedKeys[index] = dispatch.consumed;
    } else {
        if (!keyDown[index])
            return dispatch;
        keyDown[index] = false;
        dispatch.consumed = consumedKeys[index];
        consumedKeys[index] = false;
    }
    return dispatch;
}

TLoraPagerInputController::Dispatch TLoraPagerInputController::routeEncoder(int32_t delta, uint32_t timestampMs,
                                                                            const TLoraPagerKeyPolicy::Context &context)
{
    Dispatch dispatch{policy.routeEncoder(delta, timestampMs, context), false};
    dispatch.consumed = dispatchPress(dispatch.policy, context);
    return dispatch;
}

void TLoraPagerInputController::reset()
{
    policy.reset();
    for (size_t i = 0; i < 31; ++i) {
        keyDown[i] = false;
        consumedKeys[i] = false;
    }
}
