#include "input/MessageInputSession.h"

#include <cstring>
#include <string>
#include <string_view>

#include <doctest/doctest.h>

namespace
{
using Result = MessageInputSession::Result;

void type(MessageInputSession &session, std::string_view keys)
{
    for (const char key : keys)
        REQUIRE(session.insertAscii(static_cast<uint8_t>(key)) == Result::Applied);
}

void checkText(const MessageInputSession &session, std::string_view expected, bool preedit = false)
{
    const auto view = session.viewState();
    CHECK(std::string_view(view.text.data()) == expected);
    CHECK(view.hasPreedit == preedit);
    CHECK(std::strlen(view.text.data()) == session.text().payloadSize());
    CHECK(session.text().payloadSize() <= MessageTextController::MAX_PAYLOAD_BYTES);
    CHECK(MessageTextController::isValidUtf8(view.text.data()));
}
} // namespace

TEST_CASE("message input starts in English and retains mode across submitted drafts")
{
    MessageInputSession session;
    CHECK_FALSE(session.isKoreanMode());
    type(session, "rk");
    checkText(session, "rk");

    CHECK(session.toggleMode() == Result::Applied);
    CHECK(session.isKoreanMode());
    type(session, "ekfrrhrl");
    CHECK(session.prepareSubmit() == Result::Applied);
    checkText(session, "rk닭고기");
    session.clearAfterSubmit();
    CHECK(session.isKoreanMode());
    checkText(session, "");
}

TEST_CASE("composition state is visible and backspace owns it first")
{
    MessageInputSession session;
    REQUIRE(session.toggleMode() == Result::Applied);
    type(session, "ekfr");
    checkText(session, "닭", true);
    CHECK(session.backspace() == Result::Applied);
    checkText(session, "달", true);
    CHECK(session.backspace() == Result::Applied);
    checkText(session, "다", true);
    CHECK(session.backspace() == Result::Applied);
    checkText(session, "ㄷ", true);
    CHECK(session.backspace() == Result::Applied);
    checkText(session, "", false);
}

TEST_CASE("Shift Space policy can flush then toggle without inserting space")
{
    MessageInputSession session;
    REQUIRE(session.toggleMode() == Result::Applied);
    type(session, "rksk");
    checkText(session, "가나", true);
    CHECK(session.toggleMode() == Result::Applied);
    CHECK_FALSE(session.isKoreanMode());
    checkText(session, "가나", false);
}

TEST_CASE("220 byte outbound budget is transactional")
{
    MessageInputSession session;
    const std::string prefix219(219, 'a');
    REQUIRE(session.insertUtf8(prefix219.data(), prefix219.size()) == Result::Applied);
    CHECK(session.insertAscii('x') == Result::Applied);
    CHECK(session.text().payloadSize() == 220);
    CHECK(session.insertAscii('y') == Result::PayloadFull);
    checkText(session, std::string(219, 'a') + "x");

    session.reset();
    REQUIRE(session.insertUtf8(prefix219.data(), prefix219.size()) == Result::Applied);
    REQUIRE(session.toggleMode() == Result::Applied);
    CHECK(session.insertAscii('r') == Result::PayloadFull);
    checkText(session, prefix219);
    CHECK(session.backspace() == Result::Applied);
    checkText(session, std::string(218, 'a'));

    session.reset();
    const std::string prefix217(217, 'a');
    REQUIRE(session.insertUtf8(prefix217.data(), prefix217.size()) == Result::Applied);
    REQUIRE(session.toggleMode() == Result::Applied);
    type(session, "rks");
    checkText(session, prefix217 + "간", true);
    CHECK(session.insertAscii('r') == Result::PayloadFull);
    checkText(session, prefix217 + "간", true);
}

TEST_CASE("UTF-8 insertion cursor deletion and malformed input preserve invariants")
{
    MessageInputSession session;
    const std::string value = "A가😀B";
    REQUIRE(session.insertUtf8(value.data(), value.size()) == Result::Applied);
    CHECK(session.moveCursorLeft() == Result::Applied);
    CHECK(session.backspace() == Result::Applied);
    checkText(session, "A가B");

    const char malformed[] = {static_cast<char>(0xe3), static_cast<char>(0x84)};
    CHECK(session.insertUtf8(malformed, sizeof(malformed)) == Result::InvalidUtf8);
    checkText(session, "A가B");
}

TEST_CASE("representative Korean payload remains NFC UTF-8")
{
    MessageInputSession session;
    REQUIRE(session.toggleMode() == Result::Applied);
    type(session, "ekfrrhrl");
    REQUIRE(session.insertAscii(' ') == Result::Applied);
    type(session, "rkqtdl");
    REQUIRE(session.prepareSubmit() == Result::Applied);
    checkText(session, "닭고기 값이");
    CHECK(session.text().payloadSize() == 16);
}
