#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

/**
 * A fixed-state, allocation-free modern Hangul composer for the standard
 * Dubeolsik keyboard layout.
 *
 * Each editing operation returns a committed UTF-8 delta and the complete
 * current preedit. The returned buffers own their bytes and remain valid for
 * the lifetime of the returned value.
 */
class HangulComposer
{
  public:
    struct Update {
        // One operation can commit one Hangul code point plus one ASCII byte.
        std::array<char, 8> committedUtf8{};
        // Modern Hangul preedit is exactly one Unicode code point.
        std::array<char, 4> preeditUtf8{};
        uint8_t committedBytes = 0;
        uint8_t preeditBytes = 0;
        bool consumed = false;

        std::string_view committed() const { return {committedUtf8.data(), committedBytes}; }
        std::string_view preedit() const { return {preeditUtf8.data(), preeditBytes}; }
    };

    /**
     * Process one printable 7-bit ASCII key in Hangul mode.
     *
     * Dubeolsik letters are composed. Other printable ASCII commits the
     * current preedit first and then passes through in the committed delta.
     * Control bytes and non-ASCII bytes are not consumed and do not alter the
     * current composition.
     */
    Update feed(uint8_t ascii);

    /**
     * Remove one composition step. If there is no preedit, consumed is false
     * so that the caller may delete one code point from committed text.
     */
    Update backspace();

    /** Commit the current preedit and clear the composition state. */
    Update flush();

    /** Discard the current preedit without committing it. */
    void reset();

    bool empty() const { return leading_ < 0 && vowel_ < 0 && trailing_ == 0; }

  private:
    // Indices from the Unicode Hangul composition formula. trailing_ is
    // 0 for no jongseong and 1..27 otherwise.
    int8_t leading_ = -1;
    int8_t vowel_ = -1;
    uint8_t trailing_ = 0;

    uint32_t preeditCodepoint() const;
    Update currentUpdate(bool consumed) const;
};

static_assert(sizeof(HangulComposer) <= 4, "composer state must stay fixed and small");
static_assert(std::is_trivially_copyable<HangulComposer>::value, "composer must support allocation-free transactional copies");
