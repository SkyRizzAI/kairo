#pragma once
#include "kairo/hal/display.h"
#include <cstdint>
#include <cstddef>

namespace kairo {

struct BitmapFont {
    const uint8_t* data;      // glyph data: charW bytes per glyph, packed columns
    uint8_t        charW;     // pixels wide (e.g. 5)
    uint8_t        charH;     // pixels tall (e.g. 8)
    uint8_t        firstChar; // first ASCII code in data (usually 0x20)
    uint8_t        numChars;  // number of glyphs
    uint8_t        spacing;   // extra pixels between chars (usually 1)
};

// Declared in font_5x8.cpp
extern const BitmapFont FONT_5X8;

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
    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, bool on = true);
    void drawBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* bits);
    void invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);  // XOR invert

    // Text
    void setFont(const BitmapFont& font);
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

    void flush();  // delegates to driver.flush()

    // Direct RGB565 color blit — bypasses the 1-bit layer.
    // Coordinates are physical pixels (no scale). For camera/video use.
    void blitRgb565(const uint8_t* buf, uint16_t x, uint16_t y,
                    uint16_t w, uint16_t h);
    bool supportsRgb565() const;  // true if driver overrides blitRgb565

private:
    IDisplayDriver&   driver_;
    const BitmapFont* font_  = &FONT_5X8;
    float             scale_ = 1.0f;
};

} // namespace kairo
