#if HAS_TFT && defined(VIEW_480x222) && defined(MESHTASTIC_KOREAN_INPUT)

#include "graphics/fonts/HangulFont.h"
#include "fonts.h"

LV_FONT_DECLARE(lv_font_noto_sans_kr_16_ko);

namespace
{

struct FontFallbacks {
    lv_font_t montserrat12;
    lv_font_t montserrat14;

    FontFallbacks() : montserrat12(ui_font_montserrat_12), montserrat14(ui_font_montserrat_14)
    {
        montserrat12.fallback = &lv_font_noto_sans_kr_16_ko;
        montserrat14.fallback = &lv_font_noto_sans_kr_16_ko;
    }
};

FontFallbacks &fallbacks()
{
    static FontFallbacks value;
    return value;
}

} // namespace

const lv_font_t *withHangulFallback(const lv_font_t *primary)
{
    FontFallbacks &fonts = fallbacks();
    if (primary == &ui_font_montserrat_12)
        return &fonts.montserrat12;
    if (primary == &ui_font_montserrat_14)
        return &fonts.montserrat14;
    return primary;
}

#endif
