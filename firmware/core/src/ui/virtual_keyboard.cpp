#include "nema/ui/virtual_keyboard.h"
#include "nema/ui/canvas.h"
#include "nema/ui/ui_constants.h"
#include "nema/input/input_action.h"
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace nema {
} // namespace nema

namespace aether::ui {
using namespace nema;

// ── Char grid ─────────────────────────────────────────────────────────────
// Row 0: 10 keys.  Row 1: 9 keys (cols 0-8) + DEL (col 9, always).
// Row 2 alpha: CAPS (col 0) + 9 chars (cols 1-9).
// Row 2 Num/Sym: 10 chars (col 0-9, no CAPS).
static const char CHARS[4][3][11] = {
    { "QWERTYUIOP", "ASDFGHJKL",    "ZXCVBNM,." },        // Upper
    { "qwertyuiop", "asdfghjkl",    "zxcvbnm,." },        // Lower
    { "1234567890", "!@#$%^&*(",    ")-_=+[]{};"},         // Num
    { "~`|\\/<>?'\"", ":;,.!@#$%",  "&*()+=^-_~" },       // Sym
};

static constexpr int KCOLS = 10;
static constexpr int NROWS = 4;  // 3 char rows + 1 action row

// Bottom row (row 3): [MODE 0-2] [SPACE 3-6] [OK 7-8] [X 9]
static int snapR3(int col) {
    if (col <= 2) return 0;
    if (col <= 6) return 3;
    if (col <= 8) return 7;
    return 9;
}
static int moveR3(int col, int dir) {
    static const int SLOTS[] = {0, 3, 7, 9};
    int s = snapR3(col);
    for (int i = 0; i < 4; i++)
        if (SLOTS[i] == s) return SLOTS[(i + dir + 4) % 4];
    return 0;
}

static void cycleMode(VirtualKeyboard::Mode& m) {
    using M = VirtualKeyboard::Mode;
    switch (m) {
        case M::Upper:
        case M::Lower: m = M::Num;   break;
        case M::Num:   m = M::Sym;   break;
        default:       m = M::Lower; break;  // Sym → abc
    }
}

void VirtualKeyboard::clear() { buf[0] = '\0'; len = 0; row = 0; col = 0; }

void VirtualKeyboard::handle(Key k, bool& done, bool& cancel) {
    done = cancel = false;
    if (mask) field = Field::Password;

    bool isCapsMode = (mode == Mode::Upper || mode == Mode::Lower);

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
        case Key::Cancel:
            if (len > 0) backspace(); else cancel = true;
            return;
        case Key::Select: {
            int m = (int)mode;
            if (row == 0) {
                type(CHARS[m][0][col]);
            } else if (row == 1) {
                if (col == 9) backspace();
                else          type(CHARS[m][1][col]);
            } else if (row == 2) {
                if (isCapsMode && col == 0) {
                    mode = (mode == Mode::Upper) ? Mode::Lower : Mode::Upper;
                } else {
                    int ci = isCapsMode ? col - 1 : col;
                    const char* r = CHARS[m][2];
                    if (ci >= 0 && ci < (int)std::strlen(r)) type(r[ci]);
                }
            } else {  // row 3
                int s = snapR3(col);
                if (s == 0)      cycleMode(mode);
                else if (s == 3) type(' ');
                else if (s == 7) { done = true; return; }
                else             { cancel = true; return; }
            }
            break;
        }
        default: break;
    }
    if (row < 3 && col >= KCOLS) col = KCOLS - 1;
}

// ── 1D linear navigation ─────────────────────────────────────────────────
static constexpr int TOTAL_KEYS_LINEAR = 34;  // 10+10+10+4

void VirtualKeyboard::handleAction(input::Action a, bool& done, bool& cancel) {
    done = cancel = false;
    if (mask) field = Field::Password;

    int& cur = col;  // linear cursor stored in col
    bool isCapsMode = (mode == Mode::Upper || mode == Mode::Lower);

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
            if (cur < 10) {
                typeChar(CHARS[m][0][cur]);
            } else if (cur < 20) {
                int c2 = cur - 10;
                if (c2 == 9) backspace();
                else typeChar(CHARS[m][1][c2]);
            } else if (cur < 30) {
                int c2 = cur - 20;
                if (isCapsMode && c2 == 0) {
                    mode = (mode == Mode::Upper) ? Mode::Lower : Mode::Upper;
                } else {
                    int ci = isCapsMode ? c2 - 1 : c2;
                    const char* r = CHARS[m][2];
                    if (ci >= 0 && ci < (int)std::strlen(r)) typeChar(r[ci]);
                }
            } else {
                int slot = cur - 30;
                if (slot == 0)      cycleMode(mode);
                else if (slot == 1) typeChar(' ');
                else if (slot == 2) { done = true; return; }
                else                { cancel = true; return; }
            }
            break;
        }
        default: break;
    }
}

// ── Drawing ───────────────────────────────────────────────────────────────
static void drawKey(Canvas& c, int kx, int ky, int kw, int kh,
                    const char* label, bool sel) {
    if (sel) c.fillRoundRect(kx, ky, kw, kh, 2);
    int lw = (int)std::strlen(label) * nema::display::CHAR_W;
    c.drawText(kx + (kw - lw) / 2, ky + (kh - 8) / 2, label, !sel);
}

static void drawCapsKey(Canvas& c, int kx, int ky, int kw, int kh,
                        bool sel, bool capsOn) {
    if (sel) c.fillRoundRect(kx, ky, kw, kh, 2);
    // ^ = caps on (uppercase active), v = caps off
    const char* lbl = capsOn ? "^" : "v";
    c.drawText(kx + (kw - nema::display::CHAR_W) / 2, ky + (kh - 8) / 2, lbl, !sel);
}

void VirtualKeyboard::draw(Canvas& c, const char* prompt) const {
    c.clear();
    bool pw = (field == Field::Password) || mask;
    int m  = (int)mode;

    const int TOP = (int)nema::display::SEP1_Y + 2;

    // ── Keyboard geometry ────────────────────────────────────────────────
    // Cap width so the keyboard stays compact on wide/portrait screens.
    // Anchor to bottom so the prompt+field area fills whatever space remains above.
    const int KBD_MAX_W = 210;
    const int KH_MAX    = 13;
    int kbdW  = std::min((int)c.width(), KBD_MAX_W);
    int kbdX  = ((int)c.width() - kbdW) / 2;
    int STEP  = kbdW / KCOLS;
    int LEFT  = kbdX + (kbdW - STEP * KCOLS) / 2;
    // Key height: fill from top area to bottom, capped so keys don't get huge.
    int availH = (int)c.height() - TOP - 26;
    int STEPY  = std::min(availH / NROWS, KH_MAX);
    if (STEPY < 8) STEPY = 8;
    int KY  = (int)c.height() - NROWS * STEPY;  // keyboard anchored to bottom
    int KH  = STEPY;
    int KW  = STEP;

    // ── Prompt & input field ─────────────────────────────────────────────
    c.drawText(2, TOP, prompt);
    c.fillRect(0, TOP + 10, c.width(), 1);

    char shown[66];
    if (pw) { for (uint8_t i = 0; i < len; i++) shown[i] = '*'; shown[len] = '\0'; }
    else    { std::strncpy(shown, buf, sizeof(shown) - 1); shown[sizeof(shown)-1] = '\0'; }
    char field_s[72];
    std::snprintf(field_s, sizeof(field_s), "%s_", shown);
    int maxc = ((int)c.width() - 4) / (int)nema::display::CHAR_W;
    const char* fp = field_s;
    int fl = (int)std::strlen(field_s);
    if (fl > maxc) fp = field_s + (fl - maxc);
    c.drawText(2, TOP + 13, fp);

    // Thin separator line above keyboard
    c.fillRect(0, KY - 1, c.width(), 1);

    // ── Row 0 ────────────────────────────────────────────────────────────
    for (int cc = 0; cc < KCOLS; cc++) {
        char lbl[2] = { CHARS[m][0][cc], 0 };
        drawKey(c, LEFT + cc * STEP, KY, KW, KH, lbl, row == 0 && col == cc);
    }

    // ── Row 1: 9 chars + backspace ───────────────────────────────────────
    for (int cc = 0; cc < KCOLS; cc++) {
        int x = LEFT + cc * STEP, y = KY + STEPY;
        bool sel = (row == 1 && col == cc);
        if (cc == 9) drawKey(c, x, y, KW, KH, "<-", sel);
        else { char lbl[2] = { CHARS[m][1][cc], 0 }; drawKey(c, x, y, KW, KH, lbl, sel); }
    }

    // ── Row 2: CAPS + chars (alpha) or all chars (Num/Sym) ───────────────
    bool isCapsMode = (mode == Mode::Upper || mode == Mode::Lower);
    if (isCapsMode) {
        bool capsOn = (mode == Mode::Upper);
        drawCapsKey(c, LEFT, KY + 2 * STEPY, KW, KH, row == 2 && col == 0, capsOn);
        for (int cc = 1; cc <= 9; cc++) {
            char lbl[2] = { CHARS[m][2][cc - 1], 0 };
            drawKey(c, LEFT + cc * STEP, KY + 2 * STEPY, KW, KH,
                    lbl, row == 2 && col == cc);
        }
    } else {
        for (int cc = 0; cc < KCOLS; cc++) {
            char lbl[2] = { CHARS[m][2][cc], 0 };
            drawKey(c, LEFT + cc * STEP, KY + 2 * STEPY, KW, KH,
                    lbl, row == 2 && col == cc);
        }
    }

    // ── Row 3: action row ────────────────────────────────────────────────
    // [MODE 3-wide] [SPACE 4-wide] [OK 2-wide] [X 1-wide]
    int y3  = KY + 3 * STEPY;
    int r3  = (row == 3) ? snapR3(col) : -1;
    const char* modeBtn;
    switch (mode) {
        case Mode::Upper:
        case Mode::Lower: modeBtn = "123"; break;
        case Mode::Num:   modeBtn = "!@#"; break;
        default:          modeBtn = "abc"; break;
    }
    drawKey(c, LEFT,            y3, 3 * STEP, KH, modeBtn, r3 == 0);
    drawKey(c, LEFT + 3 * STEP, y3, 4 * STEP, KH, "SPACE", r3 == 3);
    drawKey(c, LEFT + 7 * STEP, y3, 2 * STEP, KH, "OK",    r3 == 7);
    drawKey(c, LEFT + 9 * STEP, y3, KW,        KH, "X",     r3 == 9);
}

} // namespace aether::ui
