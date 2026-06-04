#pragma once
#include <cstdint>

namespace kairo::ui {

// Font 5×8 metrics with 1px spacing — these are font-relative, NOT resolution-
// dependent, so they stay as constants.
constexpr uint16_t CHAR_W = 6;   // 5px glyph + 1px spacing
constexpr uint16_t CHAR_H = 9;   // 8px glyph + 1px spacing

// Zone layout — top region is anchored to the top edge, so these are fixed.
constexpr uint16_t STATUS_Y  = 3;   // status bar top — 2px inner padding from border
constexpr uint16_t STATUS_H  = 9;   // 1 char row
constexpr uint16_t SEP1_Y    = STATUS_Y + STATUS_H;  // separator after status
constexpr uint16_t CONTENT_Y = SEP1_Y + 2;

// Bottom-anchored layout is RESOLUTION-DEPENDENT — compute from canvas height.
// Pass c.height() (logical). Never hardcode a screen size.
inline uint16_t footerY    (uint16_t h) { return (uint16_t)(h - CHAR_H - 1); }
inline uint16_t sep2Y      (uint16_t h) { return (uint16_t)(h - CHAR_H - 3); }
inline uint16_t contentH   (uint16_t h) { return (uint16_t)(sep2Y(h) - CONTENT_Y); }
inline uint16_t contentRows(uint16_t h) { return contentH(h) / CHAR_H; }

// Character columns that fit a given canvas width.
inline uint16_t cols(uint16_t w) { return w / CHAR_W; }

} // namespace kairo::ui
