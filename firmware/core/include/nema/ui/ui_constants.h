#pragma once
#include <cstdint>

// Shared display layout/text-metric constants (Plan 80) — used by core (AppHost),
// the FbCon server, and Aether alike, so they live in the shared nema::display layer.
namespace nema::display {

// Font 5×8 metrics with 1px spacing — these are font-relative, NOT resolution-
// dependent, so they stay as constants.
constexpr uint16_t CHAR_W = 6;   // 5px glyph + 1px spacing
constexpr uint16_t CHAR_H = 9;   // 8px glyph + 1px spacing

// Zone layout — top region is anchored to the top edge, so these are fixed.
constexpr uint16_t STATUS_Y  = 3;   // status bar top — 2px inner padding from border
constexpr uint16_t STATUS_H  = 9;   // 1 char row
constexpr uint16_t SEP1_Y    = STATUS_Y + STATUS_H;  // separator after status
// Content starts right after the status bar — NO gap. Must equal the status bar's real
// drawn height (barH = STATUS_H + 2 in status_bar.cpp); using SEP1_Y (= STATUS_Y+STATUS_H)
// left a 1px gap because STATUS_Y(3) ≠ the bar's +2 base. There's no separator anymore.
constexpr uint16_t CONTENT_Y = STATUS_H + 2;         // = barH; full value when status bar ON

// Set to false by GuiService when display/statusbar=0; read by all layout code.
// C++17 inline variable — one definition shared across all TUs.
inline bool statusBarVisible = true;

// Dynamic content origin — 0 when status bar is hidden (reclaims the top strip).
inline uint16_t contentY() { return statusBarVisible ? CONTENT_Y : 0; }

// Bottom-anchored layout is RESOLUTION-DEPENDENT — compute from canvas height.
// Pass c.height() (logical). Never hardcode a screen size.
inline uint16_t footerY    (uint16_t h) { return (uint16_t)(h - CHAR_H - 1); }
inline uint16_t sep2Y      (uint16_t h) { return (uint16_t)(h - CHAR_H - 3); }
inline uint16_t contentH   (uint16_t h) { return (uint16_t)(sep2Y(h) - contentY()); }
inline uint16_t contentRows(uint16_t h) { return contentH(h) / CHAR_H; }

// Character columns that fit a given canvas width.
inline uint16_t cols(uint16_t w) { return w / CHAR_W; }

} // namespace aether::ui
