#include "input/TLoraPagerInputController.h"

#include <doctest/doctest.h>

namespace
{
using Action = TLoraPagerKeyPolicy::Action;
using Context = TLoraPagerKeyPolicy::Context;
using Kind = TLoraPagerKeyPolicy::ActionKind;
using Modifier = TLoraPagerKeyPolicy::Modifier;
using Raw = TLoraPagerKeyPolicy::RawEvent;

TLoraPagerKeyPolicy::Result edge(TLoraPagerKeyPolicy &policy, uint8_t key, bool pressed, uint32_t at, Context context)
{
    return policy.feed(Raw{key, pressed, at}, context);
}

void tap(TLoraPagerKeyPolicy &policy, uint8_t key, uint32_t &at, Context context, uint32_t duration = 1)
{
    policy.feed(Raw{key, true, at}, context);
    at += duration;
    policy.feed(Raw{key, false, at++}, context);
}

void checkPair(TLoraPagerKeyPolicy &policy, uint8_t key, uint32_t &at, Context context, Kind kind, uint32_t codepoint,
               Modifier modifier)
{
    const auto down = edge(policy, key, true, at++, context);
    const auto up = edge(policy, key, false, at++, context);
    REQUIRE(down.emit);
    CHECK(down.consume);
    CHECK(down.action.pressed);
    CHECK(up.emit);
    CHECK(up.consume);
    CHECK_FALSE(up.action.pressed);
    CHECK(down.action.kind == kind);
    CHECK(up.action.kind == kind);
    CHECK(down.action.codepoint == codepoint);
    CHECK(up.action.codepoint == codepoint);
    CHECK(down.action.appliedModifier == modifier);
    CHECK(up.action.appliedModifier == modifier);
}

class RecordingSink : public TLoraPagerActionSink
{
  public:
    bool handleTLoraPagerAction(const Action &action) override
    {
        calls++;
        last = action;
        return accept;
    }

    bool accept = true;
    unsigned calls = 0;
    Action last{};
};
} // namespace

TEST_CASE("Pager key policy covers text navigation modifiers and wake")
{
    constexpr Context text{true, true};
    constexpr Context navigation{true, false};
    TLoraPagerKeyPolicy policy;
    uint32_t now = 10;

    checkPair(policy, 1, now, text, Kind::Character, 'q', Modifier::None);
    tap(policy, 29, now, text);
    checkPair(policy, 1, now, text, Kind::Character, 'Q', Modifier::Shift);
    CHECK(policy.modifier() == Modifier::None);

    tap(policy, 29, now, text);
    const auto toggleDown = edge(policy, 31, true, now++, text);
    const auto toggleUp = edge(policy, 31, false, now++, text);
    CHECK(toggleDown.action.kind == Kind::ToggleInputMode);
    CHECK(toggleDown.action.codepoint == 0);
    CHECK(toggleUp.action.kind == Kind::ToggleInputMode);

    tap(policy, 29, now, navigation);
    const auto outsideDown = edge(policy, 31, true, now++, navigation);
    const auto outsideUp = edge(policy, 31, false, now++, navigation);
    CHECK(outsideDown.consume);
    CHECK_FALSE(outsideDown.emit);
    CHECK(outsideDown.action.kind == Kind::Consumed);
    CHECK(outsideUp.consume);

    checkPair(policy, 30, now, text, Kind::TextBackspace, 0x08, Modifier::None);
    checkPair(policy, 30, now, navigation, Kind::NavigateBack, 0x08, Modifier::None);
    checkPair(policy, 20, now, text, Kind::Submit, 0x0d, Modifier::None);
    checkPair(policy, 20, now, navigation, Kind::Activate, 0x0d, Modifier::None);

    tap(policy, 21, now, navigation, TLoraPagerKeyPolicy::symHoldThresholdMs - 1);
    checkPair(policy, 1, now, text, Kind::Character, '1', Modifier::Sym);
    tap(policy, 21, now, navigation);
    checkPair(policy, 20, now, navigation, Kind::NavigateNext, 0x09, Modifier::Sym);
    tap(policy, 21, now, navigation);
    checkPair(policy, 31, now, navigation, Kind::ToggleKeyboardBacklight, 0, Modifier::Sym);

    const uint32_t heldAt = now;
    edge(policy, 21, true, now++, navigation);
    CHECK(policy.routeEncoder(-1, heldAt + TLoraPagerKeyPolicy::symHoldThresholdMs, navigation).action.kind == Kind::ScrollUp);
    CHECK(policy.routeEncoder(1, heldAt + TLoraPagerKeyPolicy::symHoldThresholdMs + 1, navigation).action.kind ==
          Kind::ScrollDown);
    edge(policy, 21, false, heldAt + TLoraPagerKeyPolicy::symHoldThresholdMs + 2, navigation);
    CHECK(policy.modifier() == Modifier::None);

    const auto wakeDown = edge(policy, 2, true, now++, {false, true});
    const auto wakeUp = edge(policy, 2, false, now++, text);
    CHECK(wakeDown.action.kind == Kind::Wake);
    CHECK(wakeUp.action.kind == Kind::Wake);
    CHECK(policy.routeEncoder(1, now, {false, false}).action.kind == Kind::Wake);
}

TEST_CASE("Pager controller dispatches semantic presses once and pairs releases")
{
    TLoraPagerInputController controller;
    RecordingSink sink;
    controller.setSink(&sink);
    constexpr Context text{true, true};
    constexpr Context navigation{true, false};

    auto down = controller.feed(Raw{1, true, 1}, text);
    CHECK(down.consumed);
    CHECK(sink.calls == 1);
    CHECK(sink.last.kind == Kind::Character);
    CHECK(sink.last.codepoint == 'q');
    CHECK(controller.feed(Raw{1, true, 2}, text).consumed);
    CHECK(sink.calls == 1);
    CHECK(controller.feed(Raw{1, false, 3}, text).consumed);
    CHECK(sink.calls == 1);
    CHECK_FALSE(controller.feed(Raw{1, false, 4}, text).consumed);

    sink.accept = false;
    down = controller.feed(Raw{2, true, 5}, navigation);
    CHECK_FALSE(down.consumed);
    CHECK_FALSE(controller.feed(Raw{2, false, 6}, navigation).consumed);

    down = controller.feed(Raw{2, true, 7}, text);
    CHECK(down.consumed);
    CHECK(controller.feed(Raw{2, false, 8}, text).consumed);
}

TEST_CASE("Pager controller consumes Shift Space without inserting a space")
{
    TLoraPagerInputController controller;
    RecordingSink sink;
    controller.setSink(&sink);
    constexpr Context text{true, true};

    CHECK(controller.feed(Raw{29, true, 1}, text).consumed);
    CHECK(controller.feed(Raw{29, false, 2}, text).consumed);
    CHECK(controller.feed(Raw{31, true, 3}, text).consumed);
    CHECK(sink.calls == 1);
    CHECK(sink.last.kind == Kind::ToggleInputMode);
    CHECK(sink.last.codepoint == 0);
    CHECK(controller.feed(Raw{31, false, 4}, text).consumed);
    CHECK(sink.calls == 1);
}
