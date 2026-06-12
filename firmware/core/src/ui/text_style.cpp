#include "nema/ui/text_style.h"
#include "nema/ui/canvas.h"   // BitmapFont, FONT_5X8
#include <cstring>

namespace nema::ui {

static TextSize g_size = TextSize::Normal;

void     setTextSize(TextSize sz) { g_size = sz; }
TextSize textSize()               { return g_size; }

FontSpec fontForRole(TextRole role) {
    // Normal: Body=1x, Title=2x, Caption=1x.  Large: bump body+title one step.
    const uint8_t bump = (g_size == TextSize::Large) ? 1 : 0;
    switch (role) {
        case TextRole::Title:   return { &FONT_5X8, (uint8_t)(2 + bump) };
        case TextRole::Caption: return { &FONT_5X8, 1 };
        case TextRole::Body:
        default:                return { &FONT_5X8, (uint8_t)(1 + bump) };
    }
}

uint16_t measureTextW(const char* s, TextRole role) {
    if (!s || !*s) return 0;
    FontSpec fs = fontForRole(role);
    size_t len = std::strlen(s);
    // Matches drawTextScaled advance: (charW+spacing)*scale per glyph, minus the
    // trailing spacing so the box hugs the last glyph.
    int per  = (fs.font->charW + fs.font->spacing) * fs.scale;
    int trail = fs.font->spacing * fs.scale;
    return (uint16_t)((int)len * per - trail);
}

uint16_t measureTextH(TextRole role) {
    FontSpec fs = fontForRole(role);
    return (uint16_t)(fs.font->charH * fs.scale);
}

static uint16_t metricsW(void*, const char* t, TextRole role) { return measureTextW(t, role); }
static uint16_t metricsH(void*, TextRole role)                { return measureTextH(role); }

TextMetrics roleMetrics() {
    return TextMetrics{ metricsW, metricsH, nullptr };
}

} // namespace nema::ui
