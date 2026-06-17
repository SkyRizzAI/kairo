// Plan 60 Fase 1 — tier-1 draw toolkit implementation.
#include "nema/ui/draw.h"
#include "nema/ui/text_style.h"
#include <cstring>
#include <cstdlib>

namespace aether::ui::draw {

using nema::Canvas;
using nema::ui::TextRole;
using nema::ui::FontSpec;
using nema::ui::fontForRole;
using nema::ui::measureTextW;
using nema::ui::measureTextH;

// ── Internal helpers ──────────────────────────────────────────────────────────

static void drawText(Canvas& c, uint16_t x, uint16_t y, const char* s,
                     const FontSpec& fs, bool on = true) {
    c.setFont(*fs.font);
    if (fs.scale <= 1) c.drawText(x, y, s, on);
    else               c.drawTextScaled(x, y, s, fs.scale, on);
}

// Draw `len` chars from `text` at (x,y). Uses a stack buffer; truncates at 63 chars.
static void drawSpan(Canvas& c, uint16_t x, uint16_t y,
                     const char* text, size_t len, const FontSpec& fs, bool on = true) {
    if (!text || len == 0) return;
    char buf[64];
    if (len > 63) len = 63;
    memcpy(buf, text, len);
    buf[len] = '\0';
    drawText(c, x, y, buf, fs, on);
}

// ── Primitives ────────────────────────────────────────────────────────────────

void frame(Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    c.drawRect(x, y, w, h);
}

void box(Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool inverted) {
    if (inverted) c.invertRect(x, y, w, h);
    else          c.fillRect(x, y, w, h);
}

void box_rounded(Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool filled) {
    if (w < 2 || h < 2) return;
    if (filled) {
        c.fillRect(x, y, w, h);
        // Cut 4 corners
        c.drawPixel(x,           y,           false);
        c.drawPixel(x + w - 1,   y,           false);
        c.drawPixel(x,           y + h - 1,   false);
        c.drawPixel(x + w - 1,   y + h - 1,   false);
    } else {
        c.drawRect(x, y, w, h);
        // Cut 4 corners
        c.drawPixel(x,           y,           false);
        c.drawPixel(x + w - 1,   y,           false);
        c.drawPixel(x,           y + h - 1,   false);
        c.drawPixel(x + w - 1,   y + h - 1,   false);
    }
}

void separator(Canvas& c, uint16_t x, uint16_t y, uint16_t len, bool horizontal) {
    if (horizontal) c.fillRect(x, y, len, 1);
    else            c.fillRect(x, y, 1, len);
}

// ── Scrollbar ─────────────────────────────────────────────────────────────────

void scrollbar(Canvas& c, uint16_t x, uint16_t y, uint16_t size,
               uint16_t pos, uint16_t total, bool horizontal) {
    if (total == 0 || size == 0) return;
    const uint16_t BAR = 3;  // bar thickness in px

    // --- Dashed track ---
    if (horizontal) {
        // Track: 1px tall dots every 2px across width `size`
        for (uint16_t i = 0; i < size; i += 2)
            c.fillRect((uint16_t)(x + i), (uint16_t)(y + 1), 1, 1);
    } else {
        // Track: 1px wide dots every 2px down height `size`
        for (uint16_t i = 0; i < size; i += 2)
            c.fillRect((uint16_t)(x + 1), (uint16_t)(y + i), 1, 1);
    }

    // --- Solid thumb ---
    if (total <= 1) {
        // Full track is thumb
        if (horizontal) c.fillRect(x, y, size, BAR);
        else            c.fillRect(x, y, BAR, size);
        return;
    }

    uint16_t thumbSize = (uint16_t)((uint32_t)size / total);
    if (thumbSize < 4) thumbSize = 4;
    if (thumbSize > size) thumbSize = size;

    uint16_t maxOff  = (uint16_t)(size - thumbSize);
    uint16_t thumbOff = (uint16_t)((uint32_t)pos * maxOff / (total - 1));

    if (horizontal) {
        uint16_t tx = (uint16_t)(x + thumbOff);
        c.fillRect(tx, y, thumbSize, BAR);
    } else {
        uint16_t ty = (uint16_t)(y + thumbOff);
        c.fillRect(x, ty, BAR, thumbSize);
    }
}

// ── Text helpers ──────────────────────────────────────────────────────────────

void multiline(Canvas& c, uint16_t x, uint16_t y, uint16_t w,
               const char* text, TextRole role) {
    if (!text || !*text || w == 0) return;
    FontSpec fs = fontForRole(role);
    uint16_t charW_px = (uint16_t)((fs.font->charW + fs.font->spacing) * fs.scale);
    uint16_t lineH    = (uint16_t)(fs.font->charH * fs.scale + 1);
    uint16_t cy = y;

    size_t len = strlen(text);
    size_t lineStart = 0;
    size_t lastBreak = (size_t)-1;  // index of last safe line-break (space)
    uint16_t lineW   = 0;

    for (size_t i = 0; i <= len; i++) {
        char ch = text[i];
        bool flush = (ch == '\n' || ch == '\0');

        if (!flush) {
            if (ch == ' ') lastBreak = i;
            lineW += charW_px;
        }

        if (flush || lineW > w) {
            size_t breakAt = flush ? i
                           : (lastBreak != (size_t)-1 ? lastBreak : i);
            size_t lineLen = breakAt - lineStart;
            if (lineLen > 0)
                drawSpan(c, x, cy, text + lineStart, lineLen, fs);
            cy += lineH;

            if (!flush) {
                size_t newStart = (lastBreak != (size_t)-1) ? lastBreak + 1 : i;
                lineStart = newStart;
                lastBreak = (size_t)-1;
                // Recount lineW from new lineStart up to and including i
                lineW = (i >= lineStart) ? (uint16_t)((i - lineStart + 1) * charW_px) : 0;
            } else {
                lineStart = i + 1;
                lastBreak = (size_t)-1;
                lineW = 0;
            }
        }
    }
}

void marquee(Canvas& c, uint16_t x, uint16_t y, uint16_t w,
             const char* text, uint32_t tick, TextRole role) {
    if (!text || !*text) return;
    FontSpec fs = fontForRole(role);
    uint16_t textW = measureTextW(text, role);
    uint16_t lineH = measureTextH(role);

    if (textW <= w) {
        drawText(c, x, y, text, fs);
        return;
    }

    const uint16_t GAP = 16;  // blank pixels between repeats
    uint32_t cycle  = (uint32_t)textW + GAP;
    uint32_t offset = tick % cycle;  // pixels to scroll left

    uint16_t charW_px = (uint16_t)((fs.font->charW + fs.font->spacing) * fs.scale);

    // Find first visible char and sub-char pixel offset
    uint16_t skipChars = (uint16_t)(offset / charW_px);
    uint16_t pixOff    = (uint16_t)(offset % charW_px);

    // drawX = x - pixOff; if that would underflow, skip one more char
    uint16_t drawX;
    if (pixOff <= x) {
        drawX = (uint16_t)(x - pixOff);
    } else {
        ++skipChars;
        pixOff = 0;
        drawX = x;
    }

    size_t slen = strlen(text);
    c.setClip(x, y, w, lineH);
    if (skipChars < slen)
        drawText(c, drawX, y, text + skipChars, fs);
    c.clearClip();
}

void ellipsis(Canvas& c, uint16_t x, uint16_t y, uint16_t w,
              const char* text, TextRole role) {
    if (!text || !*text) return;
    FontSpec fs = fontForRole(role);
    uint16_t textW = measureTextW(text, role);

    if (textW <= w) {
        drawText(c, x, y, text, fs);
        return;
    }

    uint16_t dotsW = measureTextW("...", role);
    if (dotsW >= w) {
        drawText(c, x, y, "...", fs);
        return;
    }

    uint16_t available = w - dotsW;
    uint16_t charW_px  = (uint16_t)((fs.font->charW + fs.font->spacing) * fs.scale);
    size_t   n         = available / charW_px;
    size_t   len       = strlen(text);
    if (n > len) n = len;
    if (n == 0) {
        drawText(c, x, y, "...", fs);
        return;
    }

    drawSpan(c, x, y, text, n, fs);
    uint16_t partW = (uint16_t)(n * charW_px);
    drawText(c, (uint16_t)(x + partW), y, "...", fs);
}

// ── Icon ──────────────────────────────────────────────────────────────────────

void icon(Canvas& c, uint16_t x, uint16_t y,
          const uint8_t* bitmap, uint8_t w_px, uint8_t h_px) {
    if (!bitmap || w_px == 0 || h_px == 0) return;
    c.drawBitmap(x, y, w_px, h_px, bitmap);
}

// ── Chrome / DSi-style ────────────────────────────────────────────────────────

void banner(Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
            const char* title, bool notch) {
    // Filled dark bar
    c.fillRect(x, y, w, h);

    // Centered title — white text (on=false on filled background)
    if (title && *title) {
        FontSpec fs = fontForRole(nema::ui::TextRole::Title);
        uint16_t tw = measureTextW(title, nema::ui::TextRole::Title);
        uint16_t th = measureTextH(nema::ui::TextRole::Title);
        uint16_t tx = (tw < w) ? (uint16_t)(x + (w - tw) / 2) : x;
        uint16_t ty = (th < h) ? (uint16_t)(y + (h - th) / 2) : y;
        drawText(c, tx, ty, title, fs, false);
    }

    // Notch indicator: 3-pixel wide chevron below banner center
    if (notch && h >= 2) {
        uint16_t mx = (uint16_t)(x + w / 2);
        uint16_t ny = (uint16_t)(y + h);  // just below banner
        // Draw a small ▼ shape: 3 pixels wide, 2 pixels tall
        c.drawPixel(mx,       ny,     true);
        c.drawPixel((uint16_t)(mx - 1), (uint16_t)(ny - 1), true);
        c.drawPixel((uint16_t)(mx + 1), (uint16_t)(ny - 1), true);
    }
}

void posbar(Canvas& c, uint16_t x, uint16_t y, uint16_t w,
            uint16_t pos, uint16_t total) {
    if (total == 0) return;
    const uint16_t DOT = 3;   // dot size in px
    const uint16_t GAP = 2;   // gap between dots

    uint16_t totalW = (uint16_t)(total * DOT + (total > 1 ? (total - 1) * GAP : 0));
    uint16_t startX = (totalW < w) ? (uint16_t)(x + (w - totalW) / 2) : x;

    for (uint16_t i = 0; i < total; i++) {
        uint16_t dx = (uint16_t)(startX + i * (DOT + GAP));
        if (i == pos) {
            // Filled dot
            c.fillRect(dx, y, DOT, DOT);
        } else {
            // Outline dot
            c.drawRect(dx, y, DOT, DOT);
        }
    }
}

} // namespace aether::ui::draw
