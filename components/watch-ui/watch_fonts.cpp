#include "watch_fonts.h"

extern "C" {
LV_FONT_DECLARE(watch_latin_18);
LV_FONT_DECLARE(watch_latin_26);
LV_FONT_DECLARE(watch_latin_30);
}

namespace {
lv_font_t with_latin(const lv_font_t &base, const lv_font_t &latin)
{
    lv_font_t font = base;
    font.fallback = &latin;
    // Include accent ascenders/descenders so content-sized labels do not clip them.
    font.base_line = base.base_line > latin.base_line ? base.base_line : latin.base_line;
    const int base_ascent = base.line_height - base.base_line;
    const int latin_ascent = latin.line_height - latin.base_line;
    font.line_height = (base_ascent > latin_ascent ? base_ascent : latin_ascent) + font.base_line;
    return font;
}
}

const lv_font_t *watch_font_18()
{
    static const lv_font_t font = with_latin(lv_font_montserrat_18, watch_latin_18);
    return &font;
}
const lv_font_t *watch_font_26()
{
    static const lv_font_t font = with_latin(lv_font_montserrat_26, watch_latin_26);
    return &font;
}
const lv_font_t *watch_font_30()
{
    static const lv_font_t font = with_latin(lv_font_montserrat_30, watch_latin_30);
    return &font;
}
