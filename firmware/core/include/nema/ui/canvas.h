#pragma once
#include "nema/hal/display.h"
#include "nema/ui/font_registry.h"
#include <cstdint>
#include <cstddef>

namespace nema {

// Bitmap font. Glyphs are column-major: for each pixel column, `bytesPerCol`
// bytes hold the column top-to-bottom (bit 0 = topmost row; for tall glyphs the
// 2nd byte holds rows 8..15). Two layouts:
//   • Monospace (legacy): widths==nullptr, offsets==nullptr → every glyph is
//     charW columns wide, glyph i starts at data + i*charW*bytesPerCol.
//   • Proportional: widths[i] = glyph i's column count, offsets[i] = its byte
//     offset into data. charW is then the advance used only as a fallback/max.
// Trailing fields are zero-initialised for old monospace defs ({…,spacing}),
// which the drawing code reads as widths=nullptr / bytesPerCol→1 (see canvas.cpp).
struct BitmapFont {
    const uint8_t*  data;       // glyph bitmap (column-major, bytesPerCol bytes/col)
    uint8_t         charW;      // monospace width / proportional advance fallback
    uint8_t         charH;      // pixels tall (may exceed 8 when bytesPerCol==2)
    uint8_t         firstChar;  // first ASCII code in data (usually 0x20)
    uint8_t         numChars;   // number of glyphs
    uint8_t         spacing;    // extra pixels between chars (usually 1)
    const uint8_t*  widths;     // optional per-glyph width table; nullptr = monospace
    const uint16_t* offsets;    // optional per-glyph byte offset; nullptr = monospace
    uint8_t         bytesPerCol;// bytes per pixel column (1 for ≤8px, 2 for 9..16px); 0 ⇒ 1
};

// Glyph metrics helpers — single source of truth for the monospace/proportional
// and 1/2-byte-column logic, shared by Canvas and text measurement.
inline uint8_t fontBytesPerCol(const BitmapFont& f) { return f.bytesPerCol ? f.bytesPerCol : 1; }
inline uint8_t fontGlyphWidth(const BitmapFont& f, uint8_t idx) {
    return f.widths ? f.widths[idx] : f.charW;
}
inline const uint8_t* fontGlyphData(const BitmapFont& f, uint8_t idx) {
    return f.offsets ? f.data + f.offsets[idx]
                     : f.data + (size_t)idx * f.charW * fontBytesPerCol(f);
}

// Built-in fonts
extern const BitmapFont FONT_5X8;
extern const BitmapFont FONT_6X8;
// Proportional Helvetica family (Plan 79) — regular + bold at 8/10/12px
// (cell heights 11/15/17 incl. ascenders/descenders). Generated from u8g2 BDFs.
extern const BitmapFont FONT_REG8;
extern const BitmapFont FONT_BOLD8;
extern const BitmapFont FONT_REG10;
extern const BitmapFont FONT_BOLD10;
extern const BitmapFont FONT_REG12;
extern const BitmapFont FONT_BOLD12;

// Canvas — logical-pixel drawing surface over a physical display.
//
// All public coordinates are LOGICAL pixels. With scale > 1, one logical pixel
// maps to a scale×scale block of physical pixels, so a 528×352 panel at scale=2
// presents a 264×176 logical surface — identical layout, larger pixels. App code
// never sees the physical resolution; it always works in logical units.
class Canvas {
public:
    explicit Canvas(IDisplayDriver& driver, float scale = 1.0f);

    uint16_t width()  const;   // LOGICAL width  (physical / scale)
    uint16_t height() const;   // LOGICAL height (physical / scale)
    float    scale()  const { return scale_; }
    void     setScale(float s) { scale_ = s >= 1.0f ? s : 1.0f; }

    // Drawing primitives
    void clear(bool on = false);
    void drawPixel(uint16_t x, uint16_t y, bool on = true);
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on = true);
    void drawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on = true);   // outline
    // Rounded rect (Flipper-style selection box). r is the corner inset in px
    // (1 = clip the 4 corner pixels). Falls back to square for tiny rects.
    void fillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r = 1, bool on = true);
    void drawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r = 1, bool on = true); // outline
    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, bool on = true);
    void drawBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* bits);
    void invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);  // XOR invert

    // Text
    void setFont(const BitmapFont& font);             // legacy
    void setFont(ui::FontHandle handle);              // Plan 70: resolve from registry
    void drawText(uint16_t x, uint16_t y, const char* text, bool on = true);
    void drawChar(uint16_t x, uint16_t y, char ch, bool on = true);
    uint16_t textWidth(const char* text) const;
    uint16_t textHeight() const;

    // Scaled text — each glyph pixel rendered as scale×scale block
    void     drawTextScaled(uint16_t x, uint16_t y, const char* text, uint8_t scale, bool on = true);
    uint16_t textWidthScaled(const char* text, uint8_t scale) const;

    // Center helpers
    uint16_t centerX(const char* text) const;
    uint16_t centerXScaled(const char* text, uint8_t scale) const;

    // Clip region (logical px). Drawing primitives are silently clipped to this
    // rectangle. Used by the renderer to confine a ScrollView's content to its
    // viewport. Default = full canvas. Save/restore via getClip()+setClip().
    void setClip(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void clearClip();
    void getClip(uint16_t& x, uint16_t& y, uint16_t& w, uint16_t& h) const;

    void flush();  // delegates to driver.flush()

    // Direct RGB565 color blit — bypasses the 1-bit layer.
    // Coordinates are physical pixels (no scale). For camera/video use.
    void blitRgb565(const uint8_t* buf, uint16_t x, uint16_t y,
                    uint16_t w, uint16_t h);
    bool supportsRgb565() const;  // true if driver overrides blitRgb565

private:
    // True if (x,y) lies within the current clip rectangle.
    bool inClip(uint16_t x, uint16_t y) const;

    IDisplayDriver&   driver_;
    const BitmapFont* font_  = &FONT_5X8;
    float             scale_ = 1.0f;

    // Clip rectangle in LOGICAL px. clipX1_/clipY1_ are exclusive bounds.
    // Initialised lazily to the full canvas on first use (0,0 means "unset").
    uint16_t clipX0_ = 0, clipY0_ = 0;
    uint16_t clipX1_ = 0xFFFF, clipY1_ = 0xFFFF;
};

} // namespace nema
