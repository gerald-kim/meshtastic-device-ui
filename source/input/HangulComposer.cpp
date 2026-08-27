#include "input/HangulComposer.h"

#include <array>

namespace
{
constexpr uint32_t HANGUL_SYLLABLE_BASE = 0xAC00;
constexpr uint32_t COMPAT_VOWEL_BASE = 0x314F;

struct Jamo {
    enum class Kind : uint8_t { None, Consonant, Vowel };

    Kind kind = Kind::None;
    int8_t index = -1;
};

// Unicode choseong-index to compatibility-jamo code point.
constexpr std::array<uint16_t, 19> LEADING_COMPAT = {
    0x3131, // ㄱ
    0x3132, // ㄲ
    0x3134, // ㄴ
    0x3137, // ㄷ
    0x3138, // ㄸ
    0x3139, // ㄹ
    0x3141, // ㅁ
    0x3142, // ㅂ
    0x3143, // ㅃ
    0x3145, // ㅅ
    0x3146, // ㅆ
    0x3147, // ㅇ
    0x3148, // ㅈ
    0x3149, // ㅉ
    0x314A, // ㅊ
    0x314B, // ㅋ
    0x314C, // ㅌ
    0x314D, // ㅍ
    0x314E, // ㅎ
};

// Unicode jongseong-index to compatibility-jamo code point. Entry zero is
// unused because it means "no trailing consonant".
constexpr std::array<uint16_t, 28> TRAILING_COMPAT = {
    0,
    0x3131, // ㄱ
    0x3132, // ㄲ
    0x3133, // ㄳ
    0x3134, // ㄴ
    0x3135, // ㄵ
    0x3136, // ㄶ
    0x3137, // ㄷ
    0x3139, // ㄹ
    0x313A, // ㄺ
    0x313B, // ㄻ
    0x313C, // ㄼ
    0x313D, // ㄽ
    0x313E, // ㄾ
    0x313F, // ㄿ
    0x3140, // ㅀ
    0x3141, // ㅁ
    0x3142, // ㅂ
    0x3144, // ㅄ
    0x3145, // ㅅ
    0x3146, // ㅆ
    0x3147, // ㅇ
    0x3148, // ㅈ
    0x314A, // ㅊ
    0x314B, // ㅋ
    0x314C, // ㅌ
    0x314D, // ㅍ
    0x314E, // ㅎ
};

constexpr Jamo mapDubeolsik(uint8_t ascii)
{
    // Only the seven shifted keys which differ on a standard Dubeolsik
    // layout retain their case. All other ASCII letters are case-insensitive.
    switch (ascii) {
    case 'R':
        return {Jamo::Kind::Consonant, 1}; // ㄲ
    case 'E':
        return {Jamo::Kind::Consonant, 4}; // ㄸ
    case 'Q':
        return {Jamo::Kind::Consonant, 8}; // ㅃ
    case 'T':
        return {Jamo::Kind::Consonant, 10}; // ㅆ
    case 'W':
        return {Jamo::Kind::Consonant, 13}; // ㅉ
    case 'O':
        return {Jamo::Kind::Vowel, 3}; // ㅒ
    case 'P':
        return {Jamo::Kind::Vowel, 7}; // ㅖ
    default:
        break;
    }

    if (ascii >= 'A' && ascii <= 'Z')
        ascii = static_cast<uint8_t>(ascii - 'A' + 'a');

    switch (ascii) {
    case 'r':
        return {Jamo::Kind::Consonant, 0};
    case 's':
        return {Jamo::Kind::Consonant, 2};
    case 'e':
        return {Jamo::Kind::Consonant, 3};
    case 'f':
        return {Jamo::Kind::Consonant, 5};
    case 'a':
        return {Jamo::Kind::Consonant, 6};
    case 'q':
        return {Jamo::Kind::Consonant, 7};
    case 't':
        return {Jamo::Kind::Consonant, 9};
    case 'd':
        return {Jamo::Kind::Consonant, 11};
    case 'w':
        return {Jamo::Kind::Consonant, 12};
    case 'c':
        return {Jamo::Kind::Consonant, 14};
    case 'z':
        return {Jamo::Kind::Consonant, 15};
    case 'x':
        return {Jamo::Kind::Consonant, 16};
    case 'v':
        return {Jamo::Kind::Consonant, 17};
    case 'g':
        return {Jamo::Kind::Consonant, 18};
    case 'k':
        return {Jamo::Kind::Vowel, 0};
    case 'o':
        return {Jamo::Kind::Vowel, 1};
    case 'i':
        return {Jamo::Kind::Vowel, 2};
    case 'j':
        return {Jamo::Kind::Vowel, 4};
    case 'p':
        return {Jamo::Kind::Vowel, 5};
    case 'u':
        return {Jamo::Kind::Vowel, 6};
    case 'h':
        return {Jamo::Kind::Vowel, 8};
    case 'y':
        return {Jamo::Kind::Vowel, 12};
    case 'n':
        return {Jamo::Kind::Vowel, 13};
    case 'b':
        return {Jamo::Kind::Vowel, 17};
    case 'm':
        return {Jamo::Kind::Vowel, 18};
    case 'l':
        return {Jamo::Kind::Vowel, 20};
    default:
        return {};
    }
}

constexpr uint8_t leadingToTrailing(int8_t leading)
{
    constexpr std::array<uint8_t, 19> table = {
        1, 2, 4, 7, 0, 8, 16, 17, 0, 19, 20, 21, 22, 0, 23, 24, 25, 26, 27,
    };
    return table[static_cast<size_t>(leading)];
}

constexpr int8_t trailingToLeading(uint8_t trailing)
{
    constexpr std::array<int8_t, 28> table = {
        -1, 0, 1, -1, 2, -1, -1, 3, 5, -1, -1, -1, -1, -1, -1, -1, 6, 7, -1, 9, 10, 11, 12, 14, 15, 16, 17, 18,
    };
    return table[trailing];
}

constexpr int8_t combineVowel(int8_t first, int8_t second)
{
    if (first == 8) { // ㅗ
        if (second == 0)
            return 9; // ㅘ
        if (second == 1)
            return 10; // ㅙ
        if (second == 20)
            return 11;        // ㅚ
    } else if (first == 13) { // ㅜ
        if (second == 4)
            return 14; // ㅝ
        if (second == 5)
            return 15; // ㅞ
        if (second == 20)
            return 16;                        // ㅟ
    } else if (first == 18 && second == 20) { // ㅡ + ㅣ
        return 19;                            // ㅢ
    }
    return -1;
}

constexpr int8_t splitVowel(int8_t vowel)
{
    switch (vowel) {
    case 9:
    case 10:
    case 11:
        return 8;
    case 14:
    case 15:
    case 16:
        return 13;
    case 19:
        return 18;
    default:
        return -1;
    }
}

constexpr uint8_t combineTrailing(uint8_t first, uint8_t second)
{
    if (first == 1 && second == 19)
        return 3; // ㄳ
    if (first == 4 && second == 22)
        return 5; // ㄵ
    if (first == 4 && second == 27)
        return 6; // ㄶ
    if (first == 8) {
        switch (second) {
        case 1:
            return 9; // ㄺ
        case 16:
            return 10; // ㄻ
        case 17:
            return 11; // ㄼ
        case 19:
            return 12; // ㄽ
        case 25:
            return 13; // ㄾ
        case 26:
            return 14; // ㄿ
        case 27:
            return 15; // ㅀ
        default:
            break;
        }
    }
    if (first == 17 && second == 19)
        return 18; // ㅄ
    return 0;
}

struct TrailingSplit {
    uint8_t first = 0;
    int8_t secondLeading = -1;
};

constexpr TrailingSplit splitTrailing(uint8_t trailing)
{
    switch (trailing) {
    case 3:
        return {1, 9};
    case 5:
        return {4, 12};
    case 6:
        return {4, 18};
    case 9:
        return {8, 0};
    case 10:
        return {8, 6};
    case 11:
        return {8, 7};
    case 12:
        return {8, 9};
    case 13:
        return {8, 16};
    case 14:
        return {8, 17};
    case 15:
        return {8, 18};
    case 18:
        return {17, 9};
    default:
        return {};
    }
}

uint8_t encodeUtf8(uint32_t cp, char *out)
{
    if (cp <= 0x7F) {
        out[0] = static_cast<char>(cp);
        return 1;
    }
    if (cp <= 0x7FF) {
        out[0] = static_cast<char>(0xC0 | (cp >> 6));
        out[1] = static_cast<char>(0x80 | (cp & 0x3F));
        return 2;
    }
    out[0] = static_cast<char>(0xE0 | (cp >> 12));
    out[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out[2] = static_cast<char>(0x80 | (cp & 0x3F));
    return 3;
}
} // namespace

uint32_t HangulComposer::preeditCodepoint() const
{
    if (leading_ >= 0 && vowel_ >= 0)
        return HANGUL_SYLLABLE_BASE + static_cast<uint32_t>((leading_ * 21 + vowel_) * 28 + trailing_);
    if (leading_ >= 0)
        return LEADING_COMPAT[static_cast<size_t>(leading_)];
    if (vowel_ >= 0)
        return COMPAT_VOWEL_BASE + static_cast<uint32_t>(vowel_);
    if (trailing_ != 0)
        return TRAILING_COMPAT[trailing_];
    return 0;
}

HangulComposer::Update HangulComposer::currentUpdate(bool consumed) const
{
    Update update;
    update.consumed = consumed;
    const uint32_t cp = preeditCodepoint();
    if (cp != 0)
        update.preeditBytes = encodeUtf8(cp, update.preeditUtf8.data());
    return update;
}

HangulComposer::Update HangulComposer::feed(uint8_t ascii)
{
    if (ascii < 0x20 || ascii > 0x7E)
        return currentUpdate(false);

    const Jamo input = mapDubeolsik(ascii);
    if (input.kind == Jamo::Kind::None) {
        Update update;
        update.consumed = true;
        const uint32_t cp = preeditCodepoint();
        if (cp != 0)
            update.committedBytes = encodeUtf8(cp, update.committedUtf8.data());
        update.committedUtf8[update.committedBytes++] = static_cast<char>(ascii);
        reset();
        return update;
    }

    if (input.kind == Jamo::Kind::Vowel) {
        if (leading_ < 0 && vowel_ < 0 && trailing_ != 0) {
            Update update;
            update.consumed = true;
            const TrailingSplit split = splitTrailing(trailing_);
            if (split.secondLeading >= 0) {
                update.committedBytes = encodeUtf8(TRAILING_COMPAT[split.first], update.committedUtf8.data());
                leading_ = split.secondLeading;
            } else {
                leading_ = trailingToLeading(trailing_);
            }
            trailing_ = 0;
            vowel_ = input.index;
            update.preeditBytes = encodeUtf8(preeditCodepoint(), update.preeditUtf8.data());
            return update;
        }

        if (vowel_ < 0) {
            vowel_ = input.index;
            return currentUpdate(true);
        }

        if (trailing_ == 0) {
            const int8_t combined = combineVowel(vowel_, input.index);
            if (combined >= 0) {
                vowel_ = combined;
                return currentUpdate(true);
            }
        }

        Update update;
        update.consumed = true;
        if (trailing_ != 0) {
            const TrailingSplit split = splitTrailing(trailing_);
            int8_t nextLeading;
            if (split.secondLeading >= 0) {
                trailing_ = split.first;
                nextLeading = split.secondLeading;
            } else {
                nextLeading = trailingToLeading(trailing_);
                trailing_ = 0;
            }
            update.committedBytes = encodeUtf8(preeditCodepoint(), update.committedUtf8.data());
            trailing_ = 0;
            leading_ = nextLeading;
            vowel_ = input.index;
        } else {
            update.committedBytes = encodeUtf8(preeditCodepoint(), update.committedUtf8.data());
            leading_ = -1;
            vowel_ = input.index;
        }
        const uint32_t cp = preeditCodepoint();
        update.preeditBytes = encodeUtf8(cp, update.preeditUtf8.data());
        return update;
    }

    // Consonant input.
    if (empty()) {
        leading_ = input.index;
        return currentUpdate(true);
    }

    Update update;
    update.consumed = true;

    if (vowel_ < 0) {
        const uint8_t existingTrailing = trailing_ != 0 ? trailing_ : leadingToTrailing(leading_);
        const uint8_t incomingTrailing = leadingToTrailing(input.index);
        if (existingTrailing != 0 && incomingTrailing != 0) {
            const uint8_t combined = combineTrailing(existingTrailing, incomingTrailing);
            if (combined != 0) {
                leading_ = -1;
                trailing_ = combined;
                return currentUpdate(true);
            }
        }
    }

    if (vowel_ >= 0 && leading_ >= 0) {
        const uint8_t incomingTrailing = leadingToTrailing(input.index);
        if (trailing_ == 0 && incomingTrailing != 0) {
            trailing_ = incomingTrailing;
            return currentUpdate(true);
        }
        if (trailing_ != 0 && incomingTrailing != 0) {
            const uint8_t combined = combineTrailing(trailing_, incomingTrailing);
            if (combined != 0) {
                trailing_ = combined;
                return currentUpdate(true);
            }
        }
    }

    update.committedBytes = encodeUtf8(preeditCodepoint(), update.committedUtf8.data());
    leading_ = input.index;
    vowel_ = -1;
    trailing_ = 0;
    update.preeditBytes = encodeUtf8(preeditCodepoint(), update.preeditUtf8.data());
    return update;
}

HangulComposer::Update HangulComposer::backspace()
{
    if (empty())
        return currentUpdate(false);

    if (trailing_ != 0) {
        const TrailingSplit split = splitTrailing(trailing_);
        trailing_ = split.first;
    } else if (vowel_ >= 0) {
        const int8_t split = splitVowel(vowel_);
        vowel_ = split;
    } else {
        leading_ = -1;
    }
    return currentUpdate(true);
}

HangulComposer::Update HangulComposer::flush()
{
    Update update;
    const uint32_t cp = preeditCodepoint();
    if (cp != 0) {
        update.consumed = true;
        update.committedBytes = encodeUtf8(cp, update.committedUtf8.data());
    }
    reset();
    return update;
}

void HangulComposer::reset()
{
    leading_ = -1;
    vowel_ = -1;
    trailing_ = 0;
}
