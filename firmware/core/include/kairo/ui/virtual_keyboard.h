#pragma once
#include "kairo/ui/key.h"
#include "kairo/input/input_action.h"
#include <cstdint>

namespace kairo {

class Canvas;

namespace ui {

// VirtualKeyboard — full on-screen QWERTY driven by the 6 hardware buttons,
// modelled on the reference firmware keyboard (Android/Switch-style).
//
// 3 char modes: UPPER, lower, 123+symbols. Toggle alpha↔num with the [123/ABC]
// key; toggle case with the CAPS key. Bottom row: [mode] [SPACE] [OK] [ESC].
// DEL (backspace) sits at the right of row 1. Cancel button = backspace too
// (exit on empty). Pure state + draw — rendered fullscreen inside the calling
// app's own buffer (app-model safe, no ViewDispatcher). DRY across any app.
//
// Input modes: Text (plain) or Password (renders '*').
struct VirtualKeyboard {
    enum class Mode  : uint8_t { Upper = 0, Lower = 1, Num = 2 };
    enum class Field : uint8_t { Text, Password };

    char    buf[64] = {};
    uint8_t len     = 0;
    int     row     = 0;
    int     col     = 0;
    Mode    mode    = Mode::Upper;
    Field   field   = Field::Text;

    // When true, uses 1D linear cursor instead of 2D grid.
    // Set by the app based on: rt.capabilities().has("input.2d")
    bool linear = false;

    void clear();
    void setPassword(bool on) { field = on ? Field::Password : Field::Text; }

    // 2D grid navigation (default — 6-button boards with input.2d).
    void handle(Key k, bool& done, bool& cancel);

    // 1D linear navigation (3-button boards without input.2d).
    // Prev = move cursor left, Next = move right, Activate = type key,
    // Back = backspace or cancel.
    void handleAction(input::Action a, bool& done, bool& cancel);

    // Draw fullscreen: prompt + input field + keyboard.
    void draw(Canvas& c, const char* prompt) const;

    // Back-compat with the earlier simple API (WifiApp sets .mask).
    bool mask = false;  // mirrors field == Password
};

} // namespace ui
} // namespace kairo
