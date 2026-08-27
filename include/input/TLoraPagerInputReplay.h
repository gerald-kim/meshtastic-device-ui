#pragma once

#if defined(ARCH_PORTDUINO) && defined(MESHTASTIC_KOREAN_INPUT)

#include "input/TLoraPagerInputController.h"

#include <cstddef>
#include <string>
#include <vector>

/** Native-only raw-key fixture runner for the production Pager input path. */
class TLoraPagerInputReplay
{
  public:
    struct RunResult {
        bool loaded = false;
        bool valid = false;
        size_t events = 0;
        size_t consumed = 0;
    };

    bool load(const char *path);
    RunResult run(TLoraPagerInputController &controller, const TLoraPagerKeyPolicy::Context &context) const;

    bool hasExpectedText() const { return expectedTextSet; }
    const std::string &expectedText() const { return expectedDraft; }
    bool hasExpectedMessage() const { return expectedMessageSet; }
    const std::string &expectedMessage() const { return expectedBubble; }
    bool hasExpectedMode() const { return expectedModeSet; }
    bool expectsKoreanMode() const { return expectedKoreanMode; }

  private:
    std::vector<TLoraPagerKeyPolicy::RawEvent> events;
    std::string expectedDraft;
    std::string expectedBubble;
    bool expectedTextSet = false;
    bool expectedMessageSet = false;
    bool expectedModeSet = false;
    bool expectedKoreanMode = false;
    bool valid = false;
};

#endif
