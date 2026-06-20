#include "nema/ui/text_style.h"
#include "nema/ui/canvas.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/font_registry.h"
#include <cstring>

namespace aether::ui {
using namespace nema;  // Plan 80: nema core symbols (Canvas/Key/input/anim/fonts) in scope

static TextSize g_size = TextSize::Normal;
void     setTextSize(TextSize sz) { g_size = sz; }
TextSize textSize()               { return g_size; }

FontSpec fontForRole(TextRole role) {
    const nema::FontTokens& f = nema::theme().font;
    switch (role) {
        case TextRole::Title:   return { Fonts::Primary,   f.title };
        case TextRole::Subhead: return { Fonts::Bold8,     f.subhead };   // bold section header
        case TextRole::Mono:    return { Fonts::Mono,      1 };
        case TextRole::Caption: return { Fonts::Secondary, f.caption };
        case TextRole::Smart:   [[fallthrough]];
        case TextRole::Body:
        default:                return { Fonts::Secondary, f.body };
    }
}

static const BitmapFont& resolve(FontHandle h) { return *FontRegistry::instance().get(h); }

uint16_t measureTextW(const char* s, TextRole role) {
    if (!s || !*s) return 0;
    FontSpec fs = fontForRole(role);
    const auto& font = resolve(fs.handle);
    // Sum per-glyph widths (proportional fonts) — falls back to charW for
    // monospace fonts where widths==nullptr. Scale applies after summing.
    uint32_t w = 0;
    for (const char* p = s; *p; p++) {
        uint8_t c = (uint8_t)*p;
        uint8_t gw = (c >= font.firstChar && (uint8_t)(c - font.firstChar) < font.numChars)
                     ? fontGlyphWidth(font, (uint8_t)(c - font.firstChar)) : font.charW;
        w += (uint32_t)gw + font.spacing;
    }
    if (w == 0) return 0;
    return (uint16_t)((w - font.spacing) * fs.scale);
}

uint16_t measureTextH(TextRole role) {
    FontSpec fs = fontForRole(role);
    return (uint16_t)(resolve(fs.handle).charH * fs.scale);
}

static uint16_t metricsW(void*, const char* t, TextRole role) { return measureTextW(t, role); }
static uint16_t metricsH(void*, TextRole role)                { return measureTextH(role); }

TextMetrics roleMetrics() {
    return TextMetrics{ metricsW, metricsH, nullptr };
}

} // namespace aether::ui
