#include "nema/ui/canvas.h"
#include <cstdlib>  // abs
#include <cstring>
#include <cmath>    // roundf

namespace nema {

Canvas::Canvas(IDisplayDriver& driver, float scale)
    : driver_(driver), scale_(scale >= 1.0f ? scale : 1.0f) {}

uint16_t Canvas::width()  const { return (uint16_t)(driver_.width()  / scale_); }
uint16_t Canvas::height() const { return (uint16_t)(driver_.height() / scale_); }

void Canvas::clear(bool on) { driver_.clear(on); }

// ── Clip region (logical px) ───────────────────────────────────────────────
void Canvas::setClip(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    clipX0_ = x; clipY0_ = y;
    clipX1_ = (uint16_t)(x + w); clipY1_ = (uint16_t)(y + h);
}
void Canvas::clearClip() {
    clipX0_ = 0; clipY0_ = 0; clipX1_ = 0xFFFF; clipY1_ = 0xFFFF;
}
void Canvas::getClip(uint16_t& x, uint16_t& y, uint16_t& w, uint16_t& h) const {
    uint16_t cx1 = clipX1_ > width()  ? width()  : clipX1_;
    uint16_t cy1 = clipY1_ > height() ? height() : clipY1_;
    x = clipX0_; y = clipY0_;
    w = (uint16_t)(cx1 > clipX0_ ? cx1 - clipX0_ : 0);
    h = (uint16_t)(cy1 > clipY0_ ? cy1 - clipY0_ : 0);
}
bool Canvas::inClip(uint16_t x, uint16_t y) const {
    return x >= clipX0_ && x < clipX1_ && y >= clipY0_ && y < clipY1_;
}

void Canvas::drawPixel(uint16_t x, uint16_t y, bool on) {
    if (x >= width() || y >= height()) return;
    if (!inClip(x, y)) return;
    if (scale_ == 1.0f) { driver_.drawPixel(x, y, on); return; }
    auto px = (uint16_t)roundf(x * scale_);
    auto py = (uint16_t)roundf(y * scale_);
    auto ps = (uint16_t)roundf(scale_);
    if (ps < 1) ps = 1;
    driver_.fillRect(px, py, ps, ps, on);
}

// Clamp a logical rect to the current clip; returns false if fully clipped.
static bool clampToClip(int& x, int& y, int& w, int& h,
                        uint16_t cx0, uint16_t cy0, uint16_t cx1, uint16_t cy1) {
    int x1 = x + w, y1 = y + h;
    if (x < cx0) x = cx0;
    if (y < cy0) y = cy0;
    if (x1 > (int)cx1) x1 = cx1;
    if (y1 > (int)cy1) y1 = cy1;
    w = x1 - x; h = y1 - y;
    return w > 0 && h > 0;
}

void Canvas::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) {
    int ix = x, iy = y, iw = w, ih = h;
    if (!clampToClip(ix, iy, iw, ih, clipX0_, clipY0_, clipX1_, clipY1_)) return;
    x = (uint16_t)ix; y = (uint16_t)iy; w = (uint16_t)iw; h = (uint16_t)ih;
    if (scale_ == 1.0f) { driver_.fillRect(x, y, w, h, on); return; }
    auto px = (uint16_t)roundf(x * scale_);
    auto py = (uint16_t)roundf(y * scale_);
    auto pw = (uint16_t)roundf(w * scale_); if (pw < 1) pw = 1;
    auto ph = (uint16_t)roundf(h * scale_); if (ph < 1) ph = 1;
    driver_.fillRect(px, py, pw, ph, on);
}

void Canvas::drawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) {
    // Route through Canvas::fillRect so 1px borders scale too.
    fillRect(x, y,         w, 1, on);  // top
    fillRect(x, y + h - 1, w, 1, on);  // bottom
    fillRect(x,         y, 1, h, on);  // left
    fillRect(x + w - 1, y, 1, h, on);  // right
}

void Canvas::fillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           uint8_t r, bool on) {
    if (r == 0 || w < 2 * r + 1 || h < 2 * r + 1) { fillRect(x, y, w, h, on); return; }
    // Full-width middle band + top/bottom caps inset by r → the 2r×2r corners
    // are left untouched (radius-r rounding, matches Flipper's canvas_draw_rbox).
    fillRect(x,     (uint16_t)(y + r), w,                  (uint16_t)(h - 2 * r), on);
    fillRect((uint16_t)(x + r), y,     (uint16_t)(w - 2 * r), r,                  on);
    fillRect((uint16_t)(x + r), (uint16_t)(y + h - r), (uint16_t)(w - 2 * r), r,  on);
}

void Canvas::drawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           uint8_t r, bool on) {
    if (r == 0 || w < 2 * r + 1 || h < 2 * r + 1) { drawRect(x, y, w, h, on); return; }
    fillRect((uint16_t)(x + r), y,                  (uint16_t)(w - 2 * r), 1, on); // top
    fillRect((uint16_t)(x + r), (uint16_t)(y + h - 1), (uint16_t)(w - 2 * r), 1, on); // bottom
    fillRect(x,                 (uint16_t)(y + r), 1, (uint16_t)(h - 2 * r), on);  // left
    fillRect((uint16_t)(x + w - 1), (uint16_t)(y + r), 1, (uint16_t)(h - 2 * r), on); // right
}

void Canvas::drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, bool on) {
    // Bresenham
    int dx = abs((int)x1 - (int)x0), dy = abs((int)y1 - (int)y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int x = x0, y = y0;
    while (true) {
        drawPixel((uint16_t)x, (uint16_t)y, on);
        if (x == x1 && y == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
    }
}

void Canvas::drawBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* bits) {
    for (uint16_t row = 0; row < h; row++) {
        for (uint16_t col = 0; col < w; col++) {
            uint32_t bitIdx = (uint32_t)row * w + col;
            bool on = (bits[bitIdx / 8] >> (7 - (bitIdx % 8))) & 1;
            drawPixel(x + col, y + row, on);
        }
    }
}

void Canvas::invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    int ix = x, iy = y, iw = w, ih = h;
    if (!clampToClip(ix, iy, iw, ih, clipX0_, clipY0_, clipX1_, clipY1_)) return;
    x = (uint16_t)ix; y = (uint16_t)iy; w = (uint16_t)iw; h = (uint16_t)ih;
    if (scale_ == 1.0f) { driver_.invertRect(x, y, w, h); return; }
    auto px = (uint16_t)roundf(x * scale_);
    auto py = (uint16_t)roundf(y * scale_);
    auto pw = (uint16_t)roundf(w * scale_); if (pw < 1) pw = 1;
    auto ph = (uint16_t)roundf(h * scale_); if (ph < 1) ph = 1;
    driver_.invertRect(px, py, pw, ph);
}

void Canvas::setFont(const BitmapFont& font) { font_ = &font; }

void Canvas::setFont(ui::FontHandle handle) {
    font_ = ui::FontRegistry::instance().get(handle);
}

void Canvas::drawChar(uint16_t x, uint16_t y, char ch, bool on) {
    if ((uint8_t)ch < font_->firstChar) return;
    uint8_t idx = (uint8_t)ch - font_->firstChar;
    if (idx >= font_->numChars) return;
    const uint8_t* glyph = fontGlyphData(*font_, idx);
    const uint8_t  gw    = fontGlyphWidth(*font_, idx);
    const uint8_t  bpc   = fontBytesPerCol(*font_);
    // Plan 70 FPS opt: scan each column for contiguous runs of lit pixels and
    // batch them into fillRect calls instead of per-pixel drawPixel. Tall glyphs
    // (bpc==2) pack rows 0..7 in byte 0 and rows 8..15 in byte 1 of each column.
    for (uint8_t col = 0; col < gw; col++) {
        const uint8_t* cb = glyph + (size_t)col * bpc;
        uint8_t row = 0;
        while (row < font_->charH) {
            while (row < font_->charH && !(cb[row >> 3] & (1 << (row & 7)))) row++;
            if (row >= font_->charH) break;
            uint8_t runStart = row;
            while (row < font_->charH && (cb[row >> 3] & (1 << (row & 7)))) row++;
            fillRect(x + col, y + runStart, 1, (uint16_t)(row - runStart), on);
        }
    }
}

void Canvas::drawText(uint16_t x, uint16_t y, const char* text, bool on) {
    uint16_t cx = x;
    for (const char* p = text; *p; p++) {
        uint8_t c = (uint8_t)*p;
        uint8_t gw = (c >= font_->firstChar && (uint8_t)(c - font_->firstChar) < font_->numChars)
                     ? fontGlyphWidth(*font_, (uint8_t)(c - font_->firstChar)) : font_->charW;
        if (cx + gw > width()) break;
        drawChar(cx, y, *p, on);
        cx += gw + font_->spacing;
    }
}

uint16_t Canvas::textWidth(const char* text) const {
    uint32_t w = 0;
    for (const char* p = text; *p; p++) {
        uint8_t c = (uint8_t)*p;
        uint8_t gw = (c >= font_->firstChar && (uint8_t)(c - font_->firstChar) < font_->numChars)
                     ? fontGlyphWidth(*font_, (uint8_t)(c - font_->firstChar)) : font_->charW;
        w += gw + font_->spacing;
    }
    return w ? (uint16_t)(w - font_->spacing) : 0;
}

uint16_t Canvas::textHeight() const { return font_->charH; }

void Canvas::drawTextScaled(uint16_t x, uint16_t y, const char* text, uint8_t scale, bool on) {
    uint16_t cx = x;
    const uint8_t bpc = fontBytesPerCol(*font_);
    for (const char* p = text; *p; p++) {
        uint8_t c = (uint8_t)*p;
        if (c < font_->firstChar) continue;
        uint8_t idx = (uint8_t)(c - font_->firstChar);
        if (idx >= font_->numChars) continue;
        const uint8_t* glyph = fontGlyphData(*font_, idx);
        uint8_t gw = fontGlyphWidth(*font_, idx);
        for (uint8_t col = 0; col < gw; col++) {
            const uint8_t* cb = glyph + (size_t)col * bpc;
            for (uint8_t row = 0; row < font_->charH; row++) {
                if (cb[row >> 3] & (1 << (row & 7)))
                    fillRect(cx + col * scale, y + row * scale, scale, scale, on);
            }
        }
        cx += (uint16_t)(gw + font_->spacing) * scale;
    }
}

uint16_t Canvas::textWidthScaled(const char* text, uint8_t scale) const {
    uint16_t tw = textWidth(text);
    return tw ? (uint16_t)(tw * scale) : 0;
}

uint16_t Canvas::centerX(const char* text) const {
    uint16_t tw = textWidth(text);
    return tw < width() ? (width() - tw) / 2 : 0;
}

uint16_t Canvas::centerXScaled(const char* text, uint8_t scale) const {
    uint16_t tw = textWidthScaled(text, scale);
    return tw < width() ? (width() - tw) / 2 : 0;
}

void Canvas::flush() { driver_.flush(); }

void Canvas::blitRgb565(const uint8_t* buf, uint16_t x, uint16_t y,
                         uint16_t w, uint16_t h) {
    driver_.blitRgb565(buf, x, y, w, h);
}

bool Canvas::supportsRgb565() const {
    // Probe by calling with null buf — driver returns immediately,
    // but a non-overriding driver's no-op means no support.
    // Instead, we track support via a virtual indicator method.
    // Simplest: just always try; drivers that don't support it are silent.
    return true;
}

} // namespace nema
