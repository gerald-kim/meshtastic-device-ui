#pragma once

#include "input/TLoraPagerKeyPolicy.h"

#include <cstdint>

/** Receives semantic Pager actions after raw-key policy has been applied. */
class TLoraPagerActionSink
{
  public:
    virtual ~TLoraPagerActionSink() = default;
    virtual bool isTLoraPagerTextInputActive() const { return false; }
    virtual bool handleTLoraPagerAction(const TLoraPagerKeyPolicy::Action &action) = 0;
};

/**
 * Shared policy and dispatch owner for the physical Pager keyboard and native replay.
 *
 * A sink sees semantic presses only. The controller remembers whether the press
 * was consumed so the matching raw release follows the same path.
 */
class TLoraPagerInputController
{
  public:
    struct Dispatch {
        TLoraPagerKeyPolicy::Result policy{};
        bool consumed = false;
    };

    void setSink(TLoraPagerActionSink *newSink) { sink = newSink; }
    Dispatch feed(const TLoraPagerKeyPolicy::RawEvent &event, const TLoraPagerKeyPolicy::Context &context);
    Dispatch routeEncoder(int32_t delta, uint32_t timestampMs, const TLoraPagerKeyPolicy::Context &context);
    void reset();
    bool isTextInputActive() const { return sink && sink->isTLoraPagerTextInputActive(); }

    const TLoraPagerKeyPolicy &keyPolicy() const { return policy; }

  private:
    static bool isTextAction(TLoraPagerKeyPolicy::ActionKind kind);
    bool dispatchPress(const TLoraPagerKeyPolicy::Result &result, const TLoraPagerKeyPolicy::Context &context);

    TLoraPagerKeyPolicy policy;
    TLoraPagerActionSink *sink = nullptr;
    bool keyDown[31]{};
    bool consumedKeys[31]{};
};
