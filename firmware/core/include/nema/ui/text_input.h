#pragma once
#include "nema/ui/key.h"
#include <cstdint>

namespace nema {

class Canvas;

} // namespace nema

namespace aether::ui {
using namespace nema;  // Plan 80: nema core symbols (Canvas/Key/input/anim/fonts) in scope

// TextInput — a reusable 6-button character picker, drawn as an overlay inside
// an app's own buffer (no ViewDispatcher, app-model friendly). DRY: WiFi SSID &
// password use the same helper; future login/forms too.
//
// Controls: Up/Down cycle the current character, Right appends it, Left deletes
// the last char, Select submits (done), Cancel aborts.
struct TextInput {
    char    buf[64] = {};
    uint8_t len     = 0;
    int     idx     = 0;     // index into the charset
    bool    mask    = false; // true → render '*' (passwords)

    void clear();
    // Feed a key. Sets done=true on submit, cancel=true on abort.
    void handle(Key k, bool& done, bool& cancel);
    // Draw the input as a modal overlay with `prompt` above the field.
    void draw(Canvas& c, const char* prompt) const;
};

} // namespace ui

