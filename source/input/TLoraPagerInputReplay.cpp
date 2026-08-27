#include "input/TLoraPagerInputReplay.h"

#if defined(ARCH_PORTDUINO) && defined(MESHTASTIC_KOREAN_INPUT)

#include <fstream>
#include <sstream>

namespace
{
std::string remainder(std::istringstream &fields)
{
    std::string value;
    std::getline(fields, value);
    const size_t first = value.find_first_not_of(" \t");
    return first == std::string::npos ? std::string{} : value.substr(first);
}
} // namespace

bool TLoraPagerInputReplay::load(const char *path)
{
    events.clear();
    expectedDraft.clear();
    expectedBubble.clear();
    expectedTextSet = false;
    expectedMessageSet = false;
    expectedModeSet = false;
    expectedKoreanMode = false;
    valid = false;

    std::ifstream input(path ? path : "");
    if (!input)
        return false;

    std::string line;
    while (std::getline(input, line)) {
        std::istringstream fields(line);
        std::string operation;
        fields >> operation;
        if (operation.empty() || operation[0] == '#')
            continue;

        if (operation == "edge") {
            uint32_t timestamp = 0;
            unsigned int keyCode = 0;
            std::string state;
            if (!(fields >> timestamp >> state >> keyCode) || keyCode == 0 || keyCode > 31 ||
                (state != "press" && state != "release"))
                return false;
            events.push_back({static_cast<uint8_t>(keyCode), state == "press", timestamp});
        } else if (operation == "expect_text_utf8") {
            expectedDraft = remainder(fields);
            expectedTextSet = true;
        } else if (operation == "expect_message_utf8") {
            expectedBubble = remainder(fields);
            expectedMessageSet = true;
        } else if (operation == "expect_mode") {
            std::string mode;
            if (!(fields >> mode) || (mode != "english" && mode != "korean"))
                return false;
            expectedKoreanMode = mode == "korean";
            expectedModeSet = true;
        } else {
            return false;
        }
    }

    valid = !events.empty();
    return valid;
}

TLoraPagerInputReplay::RunResult TLoraPagerInputReplay::run(TLoraPagerInputController &controller,
                                                            const TLoraPagerKeyPolicy::Context &context) const
{
    RunResult result{true, valid, events.size(), 0};
    if (!valid)
        return result;

    for (const auto &event : events) {
        if (controller.feed(event, context).consumed)
            ++result.consumed;
    }
    return result;
}

#endif
