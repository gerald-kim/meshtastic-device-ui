#include "input/HangulComposer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include <doctest/doctest.h>

#ifdef HANGUL_COMPOSER_WITH_LIBHANGUL
#include <hangul.h>
#endif

namespace
{
std::string encodeUtf8(uint32_t cp)
{
    std::string out;
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

std::string composeAll(std::string_view keys)
{
    HangulComposer composer;
    std::string committed;
    for (const char key : keys) {
        const auto update = composer.feed(static_cast<uint8_t>(key));
        committed.append(update.committed());
    }
    committed.append(composer.flush().committed());
    return committed;
}

void checkBackspaces(std::string_view keys, std::initializer_list<std::string_view> expected)
{
    HangulComposer composer;
    for (const char key : keys)
        composer.feed(static_cast<uint8_t>(key));
    for (const std::string_view value : expected) {
        const auto update = composer.backspace();
        CHECK(update.preedit() == value);
    }
}

constexpr std::array<std::string_view, 19> LEADING_KEYS = {
    "r", "R", "s", "e", "E", "f", "a", "q", "Q", "t", "T", "d", "w", "W", "c", "z", "x", "v", "g",
};

constexpr std::array<std::string_view, 21> VOWEL_KEYS = {
    "k", "o", "i", "O", "j", "p", "u", "P", "h", "hk", "ho", "hl", "y", "n", "nj", "np", "nl", "b", "m", "ml", "l",
};

constexpr std::array<std::string_view, 28> TRAILING_KEYS = {
    "",   "r",  "R", "rt", "s",  "sw", "sg", "e", "f", "fr", "fa", "fq", "ft", "fx",
    "fv", "fg", "a", "q",  "qt", "t",  "T",  "d", "w", "c",  "z",  "x",  "v",  "g",
};

#ifdef HANGUL_COMPOSER_WITH_LIBHANGUL
std::string oracleUtf8(const ucschar *text)
{
    std::string out;
    for (; *text != 0; ++text)
        out += encodeUtf8(*text);
    return out;
}

class LibhangulOracle
{
  public:
    LibhangulOracle() : context_(hangul_ic_new("2")) { REQUIRE(context_ != nullptr); }
    ~LibhangulOracle() { hangul_ic_delete(context_); }

    HangulComposer::Update feed(uint8_t ascii)
    {
        HangulComposer::Update update;
        const bool consumed = hangul_ic_process(context_, ascii);
        const std::string commit = oracleUtf8(hangul_ic_get_commit_string(context_));
        const std::string preedit = oracleUtf8(hangul_ic_get_preedit_string(context_));
        std::copy(commit.begin(), commit.end(), update.committedUtf8.begin());
        update.committedBytes = static_cast<uint8_t>(commit.size());
        if (!consumed && ascii >= 0x20 && ascii <= 0x7E)
            update.committedUtf8[update.committedBytes++] = static_cast<char>(ascii);
        std::copy(preedit.begin(), preedit.end(), update.preeditUtf8.begin());
        update.preeditBytes = static_cast<uint8_t>(preedit.size());
        update.consumed = consumed || (ascii >= 0x20 && ascii <= 0x7E);
        return update;
    }

    HangulComposer::Update backspace()
    {
        HangulComposer::Update update;
        update.consumed = hangul_ic_backspace(context_);
        const std::string preedit = oracleUtf8(hangul_ic_get_preedit_string(context_));
        std::copy(preedit.begin(), preedit.end(), update.preeditUtf8.begin());
        update.preeditBytes = static_cast<uint8_t>(preedit.size());
        return update;
    }

  private:
    HangulInputContext *context_;
};

void checkSame(const HangulComposer::Update &actual, const HangulComposer::Update &oracle)
{
    CHECK(actual.committed() == oracle.committed());
    CHECK(actual.preedit() == oracle.preedit());
    CHECK(actual.consumed == oracle.consumed);
}
#endif
} // namespace

TEST_CASE("fixed Dubeolsik corpus")
{
    CHECK(composeAll("ekfrrhrl") == "닭고기");
    CHECK(composeAll("rkqtdl") == "값이");
    CHECK(composeAll("rhk") == "과");
    CHECK(composeAll("rnj") == "궈");
    CHECK(composeAll("rml") == "긔");
    CHECK(composeAll("Rk") == "까");
    CHECK(composeAll("Ek") == "따");
    CHECK(composeAll("Qk") == "빠");
    CHECK(composeAll("Tk") == "싸");
    CHECK(composeAll("Wk") == "짜");
    CHECK(composeAll("rk") == "가");
    CHECK(composeAll("rkr") == "각");
    CHECK(composeAll("glg") == "힣");
}

TEST_CASE("committed delta and preedit are separate")
{
    HangulComposer composer;
    CHECK(composer.feed('r').preedit() == "ㄱ");
    CHECK(composer.feed('k').preedit() == "가");

    const auto transition = composer.feed('s');
    CHECK(transition.committed().empty());
    CHECK(transition.preedit() == "간");

    // The next consonant is incompatible with the existing jongseong, so 간
    // becomes committed while ㄱ becomes the new preedit.
    const auto next = composer.feed('r');
    CHECK(next.committed() == "간");
    CHECK(next.preedit() == "ㄱ");
}

TEST_CASE("compound medial and trailing consonant migration")
{
    CHECK(composeAll("ekfrk") == "달가"); // 닭 + ㅏ -> 달가
    CHECK(composeAll("rkqtl") == "갑시"); // 값 + ㅣ -> 갑시
    CHECK(composeAll("rRk") == "ㄱ까");   // lowercase ㄱ does not double-stroke combine
}

TEST_CASE("backspace decomposes the current composition")
{
    checkBackspaces("rks", {"가", "ㄱ", ""});
    checkBackspaces("rhk", {"고", "ㄱ", ""});
    checkBackspaces("ekfr", {"달", "다", "ㄷ", ""});

    HangulComposer composer;
    CHECK_FALSE(composer.backspace().consumed);
    composer.feed('R');
    CHECK(composer.backspace().preedit().empty()); // shifted ㄲ is one key
}

TEST_CASE("isolated jamo, pass-through, control keys, flush, and reset")
{
    CHECK(composeAll("rr") == "ㄱㄱ");
    CHECK(composeAll("hk") == "ㅘ");
    CHECK(composeAll("qt") == "ㅄ");
    CHECK(composeAll("qtl") == "ㅂ시");

    HangulComposer composer;
    composer.feed('r');
    composer.feed('k');
    const auto space = composer.feed(' ');
    CHECK(space.committed() == "가 ");
    CHECK(space.preedit().empty());

    composer.feed('s');
    const auto ignored = composer.feed(0x08);
    CHECK_FALSE(ignored.consumed);
    CHECK(ignored.preedit() == "ㄴ");

    const auto flushed = composer.flush();
    CHECK(flushed.committed() == "ㄴ");
    CHECK(composer.empty());

    composer.feed('r');
    composer.reset();
    CHECK(composer.empty());
    CHECK(composer.flush().committed().empty());
}

TEST_CASE("all 19 x 21 x 28 modern Hangul syllables follow the Unicode formula")
{
    for (size_t leading = 0; leading < LEADING_KEYS.size(); ++leading) {
        for (size_t vowel = 0; vowel < VOWEL_KEYS.size(); ++vowel) {
            for (size_t trailing = 0; trailing < TRAILING_KEYS.size(); ++trailing) {
                HangulComposer composer;
                HangulComposer::Update update;
                const std::string keys =
                    std::string(LEADING_KEYS[leading]) + std::string(VOWEL_KEYS[vowel]) + std::string(TRAILING_KEYS[trailing]);
                for (const char key : keys)
                    update = composer.feed(static_cast<uint8_t>(key));
                const uint32_t expected = 0xAC00 + static_cast<uint32_t>((leading * 21 + vowel) * 28 + trailing);
                INFO("keys=", keys, " codepoint=", expected);
                CHECK(update.committed().empty());
                CHECK(update.preedit() == encodeUtf8(expected));
            }
        }
    }
}

#ifdef HANGUL_COMPOSER_WITH_LIBHANGUL
TEST_CASE("libhangul 0.2.0 differential oracle")
{
    constexpr std::array<std::string_view, 13> sequences = {
        "ekfrrhrl", "rkqtdl", "rhk", "rnj", "rml", "RkEkQkTkWk", "rkrtk", "rksk", "hk", "rr", "rk 123!", "glg", "qtnl",
    };

    for (const auto sequence : sequences) {
        HangulComposer composer;
        LibhangulOracle oracle;
        for (const char key : sequence)
            checkSame(composer.feed(static_cast<uint8_t>(key)), oracle.feed(static_cast<uint8_t>(key)));
    }

    // Differential-check every key transition used to produce all 11,172
    // precomposed modern syllables.
    for (const auto leading : LEADING_KEYS) {
        for (const auto vowel : VOWEL_KEYS) {
            for (const auto trailing : TRAILING_KEYS) {
                HangulComposer composer;
                LibhangulOracle oracle;
                const std::string keys = std::string(leading) + std::string(vowel) + std::string(trailing);
                for (const char key : keys)
                    checkSame(composer.feed(static_cast<uint8_t>(key)), oracle.feed(static_cast<uint8_t>(key)));
            }
        }
    }
}

TEST_CASE("libhangul backspace differential")
{
    constexpr std::array<std::string_view, 8> sequences = {"rks", "rhk", "ekfr", "Rk", "Tk", "hk", "rkqt", "nj"};
    for (const auto sequence : sequences) {
        HangulComposer composer;
        LibhangulOracle oracle;
        for (const char key : sequence)
            checkSame(composer.feed(static_cast<uint8_t>(key)), oracle.feed(static_cast<uint8_t>(key)));
        while (!composer.empty())
            checkSame(composer.backspace(), oracle.backspace());
        checkSame(composer.backspace(), oracle.backspace());
    }
}

TEST_CASE("libhangul deterministic mixed-input differential")
{
    constexpr std::string_view keys = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ 123!.,?";
    HangulComposer composer;
    LibhangulOracle oracle;
    uint32_t random = 0x48414E47;

    for (size_t step = 0; step < 50000; ++step) {
        random = random * 1664525U + 1013904223U;
        if ((random & 7U) == 0) {
            INFO("step=", step, " backspace");
            checkSame(composer.backspace(), oracle.backspace());
        } else {
            const uint8_t key = static_cast<uint8_t>(keys[random % keys.size()]);
            INFO("step=", step, " key=", static_cast<char>(key));
            checkSame(composer.feed(key), oracle.feed(key));
        }
    }
}
#endif
