#pragma once
// Plan 60 Fase 1 — Tier-1 draw toolkit.
// All visual rendering goes through this namespace.
// Read aether::theme() for spacing/style; no hard-coded pixel constants.
#include "nema/ui/canvas.h"
#include "nema/ui/node.h"
#include <cstdint>

namespace aether::ui::draw {

// ── Primitives ────────────────────────────────────────────────────────────────

// 1-px border outline.
void frame(nema::Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h);

// Filled solid box. inverted=true → XOR (invert pixels).
void box(nema::Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
         bool inverted = false);

// Flipper-style slightly-rounded box: 1-px corner cuts.
// filled=true → solid fill; filled=false → outline only.
void box_rounded(nema::Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 bool filled = true);

// 1-px separator line. horizontal=true → horizontal bar, false → vertical bar.
void separator(nema::Canvas& c, uint16_t x, uint16_t y, uint16_t len,
               bool horizontal = true);

// ── Scrollbar ─────────────────────────────────────────────────────────────────

// Dashed track + solid thumb (Flipper-style). All lengths in pixels:
//   size         = track length (the on-screen viewport length along the axis)
//   scrollOffset = current scroll position, 0..(content-viewport)
//   viewport     = visible content length (usually == size)
//   content      = total content length
// Thumb shrinks proportionally as content grows (clamped to a minimum), and its
// far edge never passes the track end (no overshoot).
// horizontal=false → vertical bar (3px wide) at (x,y); true → horizontal (3px tall).
void scrollbar(nema::Canvas& c, uint16_t x, uint16_t y, uint16_t size,
               uint16_t scrollOffset, uint16_t viewport, uint16_t content,
               bool horizontal = false);

// ── Text helpers ──────────────────────────────────────────────────────────────

// Word-wrapped text within w pixels. Newlines advance by font height + 1.
void multiline(nema::Canvas& c, uint16_t x, uint16_t y, uint16_t w,
               const char* text, aether::ui::TextRole role = aether::ui::TextRole::Body);

// Measure the pixel height of word-wrapped text within w pixels.
// Returns single-line font height if w == 0 or text is nullptr.
// Used by layout.cpp to size wrapped Text nodes.
uint16_t measureMultilineH(const char* text, uint16_t w, TextRole role = TextRole::Body);

// Tick-driven marquee scroll. Clips text to w pixels, scrolling left each tick.
// When text fits, draws statically. tick must be incremented by caller each frame.
void marquee(nema::Canvas& c, uint16_t x, uint16_t y, uint16_t w,
             const char* text, uint32_t tick,
             aether::ui::TextRole role = aether::ui::TextRole::Body);

// Truncate with "…" if text overflows w pixels.
void ellipsis(nema::Canvas& c, uint16_t x, uint16_t y, uint16_t w,
              const char* text,
              aether::ui::TextRole role = aether::ui::TextRole::Body);

// ── Icon ──────────────────────────────────────────────────────────────────────

// Draw a 1-bit packed icon bitmap (row-major, MSB first). w_px/h_px = source size.
void icon(nema::Canvas& c, uint16_t x, uint16_t y,
          const uint8_t* bitmap, uint8_t w_px, uint8_t h_px);

// ── Chrome / DSi-style ────────────────────────────────────────────────────────

// Filled bar with centered title (inverted text). notch=true draws a small ▼
// indicator pixel-group at the bottom-center (signals scroll/continue).
void banner(nema::Canvas& c, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
            const char* title, bool notch = false);

// Row of `total` small dots (2×2) evenly spaced in width w. Current `pos` is filled;
// others are outlines. Used for DSi carousel position indicator.
void posbar(nema::Canvas& c, uint16_t x, uint16_t y, uint16_t w,
            uint16_t pos, uint16_t total);

} // namespace aether::ui::draw
