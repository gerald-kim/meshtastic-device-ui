#pragma once

#include "lvgl.h"

#if HAS_TFT && defined(VIEW_480x222) && defined(MESHTASTIC_KOREAN_INPUT)

/** Return the existing Montserrat font with Noto Sans KR as its Korean fallback. */
const lv_font_t *withHangulFallback(const lv_font_t *primary);

#endif
