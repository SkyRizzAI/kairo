#include "nema/ui/text_input.h"
#include "nema/ui/canvas.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/components.h"
#include <cstring>
#include <cstdio>

namespace nema {
} // namespace nema

namespace aether::ui {
using namespace nema;  // Plan 80: nema core symbols (Canvas/Key/input/anim/fonts) in scope

// charset: lowercase, uppercase, digits, common symbols. Last slot = DONE.
static const char* CHARSET =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "._-@#!$%&*()+=:/? ";
static int charsetLen() { return (int)std::strlen(CHARSET); }

void TextInput::clear() { buf[0] = '\0'; len = 0; idx = 0; }

void TextInput::handle(Key k, bool& done, bool& cancel) {
    done = cancel = false;
    int n = charsetLen();
    switch (k) {
        case Key::Up:    idx = (idx - 1 + n) % n; break;
        case Key::Down:  idx = (idx + 1) % n;     break;
        case Key::Right:
            if (len < sizeof(buf) - 1) { buf[len++] = CHARSET[idx]; buf[len] = '\0'; }
            break;
        case Key::Left:
            if (len > 0) { buf[--len] = '\0'; }
            break;
        case Key::Select: done   = true; break;
        case Key::Cancel: cancel = true; break;
        default: break;
    }
}

void TextInput::draw(Canvas& c, const char* prompt) const {
    const uint16_t w = 230, h = 80;
    aether::ui::drawModalBox(c, w, h);
    uint16_t mx = aether::ui::modalOriginX(c, w);
    uint16_t my = aether::ui::modalOriginY(c, h);

    c.drawText(mx + 8, my + 8, prompt);

    // Current value (masked for passwords) + caret.
    char shown[66];
    if (mask) { for (uint8_t i = 0; i < len; i++) shown[i] = '*'; shown[len] = '\0'; }
    else      { std::strncpy(shown, buf, sizeof(shown) - 1); shown[sizeof(shown)-1] = '\0'; }
    char field[72];
    std::snprintf(field, sizeof(field), "[%s_]", shown);
    c.drawText(mx + 8, my + 24, field);

    // Current pick — big, highlighted.
    char pick[2] = { CHARSET[idx], '\0' };
    const char* label = (CHARSET[idx] == ' ') ? "space" : pick;
    uint16_t px = mx + w / 2 - c.textWidth(label) / 2;
    uint16_t py = my + 44;
    c.invertRect(px - 4, py - 2, c.textWidth(label) + 8, nema::display::CHAR_H + 4);
    c.drawText(px, py, label, false);

    c.drawText(mx + 8, my + h - nema::display::CHAR_H - 6,
               "UP/DN pick  >add <del  OK done");
}

} // namespace ui

