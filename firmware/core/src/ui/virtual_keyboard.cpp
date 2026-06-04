#include "kairo/ui/virtual_keyboard.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/input/input_action.h"
#include <cstring>
#include <cstdio>

namespace kairo {
namespace ui {

// ── Char grid (rows 0-2) ──────────────────────────────────────────────────
// Row 0: 10 keys.  Row 1: 9 keys (cols 0-8) + DEL (col 9).
// Row 2 ALPHA: CAPS (col 0) + 9 chars (cols 1-9).  Row 2 NUM: 10 chars.
static const char CHARS[3][3][11] = {
    { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM,." },   // Upper
    { "qwertyuiop", "asdfghjkl", "zxcvbnm,." },   // lower
    { "1234567890", "@#$%!&*()", "-_+=;:'/?." },   // Num+symbols
};

static constexpr int KCOLS  = 10;
static constexpr int NROWS = 4;   // 3 char rows + bottom action row

// Bottom row (row 3) logical keys: [mode](span4) [SPACE](span4) [OK] [ESC]
static int snapR3(int col) {
    if (col <= 3) return 0;
    if (col <= 7) return 4;
    if (col == 8) return 8;
    return 9;
}
static int moveR3(int col, int dir) {
    static const int SLOTS[] = {0, 4, 8, 9};
    int s = snapR3(col);
    for (int i = 0; i < 4; i++)
        if (SLOTS[i] == s) return SLOTS[(i + dir + 4) % 4];
    return 0;
}

void VirtualKeyboard::clear() { buf[0] = '\0'; len = 0; row = 0; col = 0; }

void VirtualKeyboard::handle(Key k, bool& done, bool& cancel) {
    done = cancel = false;
    if (mask) field = Field::Password;          // honour back-compat flag

    auto backspace = [&]{ if (len > 0) buf[--len] = '\0'; };
    auto type = [&](char ch){ if (len < (int)sizeof(buf) - 1) { buf[len++] = ch; buf[len] = '\0'; } };

    switch (k) {
        case Key::Up:    row = (row - 1 + NROWS) % NROWS; break;
        case Key::Down:  row = (row + 1) % NROWS;         break;
        case Key::Left:
            if (row == 3) col = moveR3(col, -1);
            else          col = (col - 1 + KCOLS) % KCOLS;
            break;
        case Key::Right:
            if (row == 3) col = moveR3(col, +1);
            else          col = (col + 1) % KCOLS;
            break;
        case Key::Cancel:                            // hardware back = backspace / exit
            if (len > 0) backspace(); else cancel = true;
            return;
        case Key::Select: {
            int m = (int)mode;
            if (row == 0) { type(CHARS[m][0][col]); }
            else if (row == 1) {
                if (col == 9) backspace();           // DEL
                else          type(CHARS[m][1][col]);
            } else if (row == 2) {
                if (mode != Mode::Num && col == 0) { // CAPS toggle
                    mode = (mode == Mode::Upper) ? Mode::Lower : Mode::Upper;
                } else {
                    int ci = (mode != Mode::Num) ? col - 1 : col;
                    const char* r = CHARS[m][2];
                    if (ci >= 0 && ci < (int)std::strlen(r)) type(r[ci]);
                }
            } else { // row 3
                int s = snapR3(col);
                if (s == 0)      mode = (mode == Mode::Num) ? Mode::Upper : Mode::Num; // mode toggle
                else if (s == 4) type(' ');                                            // space
                else if (s == 8) { done = true; return; }                             // OK
                else             { cancel = true; return; }                           // ESC
            }
            break;
        }
        default: break;
    }
    // clamp col to row length for char rows
    if (row < 3) { if (col >= KCOLS) col = KCOLS - 1; }
}

// ── 1D linear navigation (3-button boards) ───────────────────────────────
// Total key count for linear cursor: 10 + 10 + 10 + 4 action slots = 34
static constexpr int TOTAL_KEYS_LINEAR = 34;

void VirtualKeyboard::handleAction(input::Action a, bool& done, bool& cancel) {
    done = cancel = false;
    if (mask) field = Field::Password;

    // linearCursor_ is stored in `col` when linear=true (row is unused)
    int& cur = col;

    auto backspace = [&]{ if (len > 0) { buf[--len] = '\0'; } };
    auto typeChar = [&](char ch) {
        if (ch && len < (int)sizeof(buf) - 1) { buf[len++] = ch; buf[len] = '\0'; }
    };

    switch (a) {
        case input::Action::Prev:
            cur = (cur - 1 + TOTAL_KEYS_LINEAR) % TOTAL_KEYS_LINEAR;
            break;
        case input::Action::Next:
            cur = (cur + 1) % TOTAL_KEYS_LINEAR;
            break;
        case input::Action::Back:
            if (len > 0) backspace(); else cancel = true;
            return;
        case input::Action::Activate: {
            int m = (int)mode;
            if (cur < 10) {                          // row 0
                typeChar(CHARS[m][0][cur]);
            } else if (cur < 20) {                   // row 1
                int c2 = cur - 10;
                if (c2 == 9) backspace();
                else typeChar(CHARS[m][1][c2]);
            } else if (cur < 30) {                   // row 2
                int c2 = cur - 20;
                if (mode != Mode::Num && c2 == 0)
                    mode = (mode == Mode::Upper) ? Mode::Lower : Mode::Upper;
                else {
                    int ci = (mode != Mode::Num) ? c2 - 1 : c2;
                    const char* r = CHARS[m][2];
                    if (ci >= 0 && ci < (int)std::strlen(r)) typeChar(r[ci]);
                }
            } else {                                 // row 3 action keys
                int slot = cur - 30;
                if (slot == 0) mode = (mode == Mode::Num) ? Mode::Upper : Mode::Num;
                else if (slot == 1) typeChar(' ');
                else if (slot == 2) { done = true; return; }
                else { cancel = true; return; }
            }
            break;
        }
        default: break;
    }
}

// ── Drawing ───────────────────────────────────────────────────────────────
static void drawKey(Canvas& c, int kx, int ky, int kw, int kh,
                    const char* label, bool sel) {
    if (sel) { c.fillRect(kx, ky, kw, kh, true); }
    else     { c.drawRect(kx, ky, kw, kh); }
    int lw = (int)std::strlen(label) * ui::CHAR_W;
    c.drawText(kx + (kw - lw) / 2, ky + (kh - 8) / 2, label, !sel);
}

// CapsLock key: cursor selection handled separately from caps-active indicator.
// When caps is active a small filled dot appears to the left of the label —
// this avoids the "key looks selected" confusion when caps is on but cursor
// is elsewhere.
static void drawCapsKey(Canvas& c, int kx, int ky, int kw, int kh,
                        bool sel, bool capsOn) {
    if (sel) { c.fillRect(kx, ky, kw, kh, true); }
    else     { c.drawRect(kx, ky, kw, kh); }

    // "Ca" label — centered, same as normal keys
    const int lw = 2 * ui::CHAR_W;
    c.drawText(kx + (kw - lw) / 2, ky + (kh - 8) / 2, "Ca", !sel);

    // Indicator dot: 3×3 filled circle to the left of the label
    if (capsOn) {
        int dotX = kx + 2;
        int dotY = ky + (kh - 3) / 2;
        c.fillRect(dotX, dotY, 3, 3, !sel);  // black on white, or white on black
    }
}

void VirtualKeyboard::draw(Canvas& c, const char* prompt) const {
    c.clear();
    bool pw = (field == Field::Password) || mask;
    int m = (int)mode;

    // The host blits Normal-mode app frames below the status-bar strip, so keep
    // all keyboard content below SEP1_Y (~y15) — otherwise the top is clipped.
    const int TOP = ui::SEP1_Y + 3;

    // Header: prompt
    c.drawText(2, TOP, prompt);
    c.fillRect(0, TOP + 10, c.width(), 1);

    // Input field
    char shown[66];
    if (pw) { for (uint8_t i = 0; i < len; i++) shown[i] = '*'; shown[len] = '\0'; }
    else    { std::strncpy(shown, buf, sizeof(shown) - 1); shown[sizeof(shown)-1] = '\0'; }
    char field_s[72];
    std::snprintf(field_s, sizeof(field_s), "%s_", shown);
    int maxc = (c.width() - 6) / ui::CHAR_W;
    const char* fp = field_s;
    int fl = (int)std::strlen(field_s);
    if (fl > maxc) fp = field_s + (fl - maxc);
    c.drawText(3, TOP + 13, fp);
    c.fillRect(0, TOP + 24, c.width(), 1);

    // Keyboard grid — sizes derived from canvas so it fills any resolution.
    // 10 columns across the width; 4 rows fill the height below the field.
    const int KY    = TOP + 27;
    const int STEP  = c.width() / KCOLS;                 // horizontal step per key
    const int LEFT  = (c.width() - STEP * KCOLS) / 2;    // center the grid
    const int availH = (int)c.height() - KY;             // vertical room for 4 rows
    const int STEPY = availH / NROWS;                    // vertical step per row
    const int KW    = STEP;                              // key cell width
    const int KH    = STEPY;                             // key cell height

    // Row 0
    for (int cc = 0; cc < KCOLS; cc++) {
        char lbl[2] = { CHARS[m][0][cc], 0 };
        drawKey(c, LEFT + cc * STEP, KY, KW - 1, KH - 1, lbl, row == 0 && col == cc);
    }
    // Row 1: 9 chars + DEL
    for (int cc = 0; cc < KCOLS; cc++) {
        int x = LEFT + cc * STEP, y = KY + STEPY;
        bool sel = (row == 1 && col == cc);
        if (cc == 9) drawKey(c, x, y, KW - 1, KH - 1, "<x", sel);
        else { char lbl[2] = { CHARS[m][1][cc], 0 }; drawKey(c, x, y, KW - 1, KH - 1, lbl, sel); }
    }
    // Row 2: CAPS (alpha) or full 10 (num)
    if (mode != Mode::Num) {
        bool capsOn = (mode == Mode::Upper);
        drawCapsKey(c, LEFT, KY + 2 * STEPY, KW - 1, KH - 1,
                    row == 2 && col == 0, capsOn);
        for (int cc = 1; cc <= 9; cc++) {
            char lbl[2] = { CHARS[m][2][cc - 1], 0 };
            drawKey(c, LEFT + cc * STEP, KY + 2 * STEPY, KW - 1, KH - 1, lbl, row == 2 && col == cc);
        }
    } else {
        for (int cc = 0; cc < KCOLS; cc++) {
            char lbl[2] = { CHARS[m][2][cc], 0 };
            drawKey(c, LEFT + cc * STEP, KY + 2 * STEPY, KW - 1, KH - 1, lbl, row == 2 && col == cc);
        }
    }
    // Row 3: [mode][SPACE][OK][ESC]
    int y3 = KY + 3 * STEPY, r3 = (row == 3) ? snapR3(col) : -1;
    const char* modeLbl = (mode == Mode::Num) ? "ABC" : "123";
    drawKey(c, LEFT,            y3, 4 * STEP - 1, KH - 1, modeLbl, r3 == 0);
    drawKey(c, LEFT + 4 * STEP, y3, 4 * STEP - 1, KH - 1, "SPACE", r3 == 4);
    drawKey(c, LEFT + 8 * STEP, y3, KW - 1,       KH - 1, "OK",    r3 == 8);
    drawKey(c, LEFT + 9 * STEP, y3, KW - 1,       KH - 1, "Es",    r3 == 9);
}

} // namespace ui
} // namespace kairo
