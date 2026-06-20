#include "nema/ui/fbcon_server.h"
#include "nema/ui/canvas.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/runtime.h"
#include "nema/system/system_info.h"
#include "nema/services/cli_service.h"
#include "nema/input/input_action.h"
#include <cstring>
#include <string>

namespace nema {

// ── Keyboard layout ───────────────────────────────────────────────────────────
// 4 char rows × 10 keys, + 1 action row with 3 wide keys.
// Special chars in char rows: '-', '_', '.', '/' for CLI use.
// Action row: SPACE (cols 0-3), DEL (cols 4-6), ENT (cols 7-9).
static constexpr int KBD_CHAR_ROWS = 4;
static constexpr int KBD_COLS      = 10;
static constexpr const char KBD_CHARS[KBD_CHAR_ROWS][KBD_COLS + 1] = {
    "qwertyuiop",
    "asdfghjkl-",
    "zxcvbnm_./",
    "1234567890",
};

// Row 4: 3 wide keys spanning the same pixel width as the 10-key rows.
// Internally stored as col = 0, 4, 7 for navigation purposes.
static constexpr int  ACT_SP  = 0;   // SPACE  (cols 0-3)
static constexpr int  ACT_DEL = 4;   // DEL    (cols 4-6)
static constexpr int  ACT_ENT = 7;   // ENTER  (cols 7-9)
static int snapActionCol(int col) {
    if (col < 4) return ACT_SP;
    if (col < 7) return ACT_DEL;
    return ACT_ENT;
}

static constexpr uint16_t KEY_W  = aether::ui::CHAR_W + 2;          // 8 px per cell
static constexpr uint16_t KEY_H  = aether::ui::CHAR_H + 2;          // 11 px per cell
static constexpr uint16_t KBD_W  = KBD_COLS * KEY_W;        // 80 px wide
static constexpr uint16_t KBD_H  = (KBD_CHAR_ROWS + 1) * KEY_H; // 55 px tall

// Enter key position (row 4, action col 7) — cursor starts here so pressing
// OK immediately submits the pre-filled "display start aether".
static constexpr int INIT_ROW = KBD_CHAR_ROWS;   // 4
static constexpr int INIT_COL = ACT_ENT;          // 7

static constexpr const char* DEFAULT_CMD = "display start aether";

// ── Constructor ───────────────────────────────────────────────────────────────

FbconServer::FbconServer(Runtime& rt) : rt_(rt) {
    inputBuf_ = DEFAULT_CMD;
    kbdOpen_  = true;
    kbdRow_   = INIT_ROW;
    kbdCol_   = INIT_COL;
    session_.id  = 0xFF;
    session_.out = [this](const std::string& line) {
        outputLines_.push_back(line);
        rt_.view().requestRedraw();
    };
}

// ── renderFrame ───────────────────────────────────────────────────────────────

void FbconServer::renderFrame(Canvas& c, ViewDispatcher&, const StatusBarData&) {
    // Text console renders with the default theme (ADR 0002: each server applies
    // its own; this keeps Aether's theme from bleeding in after a server switch).
    aether::setTheme(aether::defaultTheme());
    c.clear();

    const uint16_t lh = aether::ui::CHAR_H + 1;
    if (c.height() < lh * 2) { c.flush(); return; }

    const uint16_t kbdH    = kbdOpen_ ? KBD_H : 0;
    const uint16_t promptY = c.height() - lh - kbdH;

    // ── Banner ────────────────────────────────────────────────────────────────
    const SystemInfo& info = rt_.info();
    c.drawText(1, 1,      (info.boardName + "  v" + info.firmwareVersion).c_str(), true);
    c.drawText(1, 1 + lh, info.platformName.c_str(), true);

    // ── Output lines (fill below banner, clipped above prompt) ───────────────
    const uint16_t outputTop = 1 + lh * 2;
    if (promptY > outputTop && !outputLines_.empty()) {
        uint16_t slots = (promptY - outputTop) / lh;
        size_t   total = outputLines_.size();
        size_t   start = (total > slots) ? total - slots : 0;
        uint16_t y     = outputTop;
        for (size_t i = start; i < total; i++) {
            c.drawText(1, y, outputLines_[i].c_str(), true);
            y += lh;
        }
    }

    // ── Prompt ────────────────────────────────────────────────────────────────
    c.drawText(1, promptY, ("> " + inputBuf_).c_str(), true);

    // ── Keyboard ─────────────────────────────────────────────────────────────
    if (kbdOpen_) drawKeyboard(c);

    c.flush();
}

// ── drawKeyboard ──────────────────────────────────────────────────────────────

void FbconServer::drawKeyboard(Canvas& c) const {
    // Centered horizontally, pinned to the bottom.
    const uint16_t startX = (c.width() > KBD_W) ? (c.width() - KBD_W) / 2 : 0;
    const uint16_t startY = (c.height() >= KBD_H) ? c.height() - KBD_H : 0;

    // Clear background strip
    c.fillRect(0, startY, c.width(), KBD_H, false);

    // Helper: draw one key cell. Selected key = white fill + dark glyph.
    auto cell = [&](uint16_t cx, uint16_t cy, uint16_t cw, const char* label, bool sel) {
        uint16_t tx = cx + (cw - (uint16_t)(std::strlen(label) * aether::ui::CHAR_W)) / 2;
        uint16_t ty = cy + 1;
        if (sel) {
            c.fillRect(cx, cy, cw, KEY_H, true);
            c.drawText(tx, ty, label, false);
        } else {
            c.drawText(tx, ty, label, true);
        }
    };

    // Char rows 0-3
    for (int r = 0; r < KBD_CHAR_ROWS; r++) {
        uint16_t ry = startY + (uint16_t)r * KEY_H;
        for (int cc = 0; cc < KBD_COLS; cc++) {
            char lbl[2] = { KBD_CHARS[r][cc], '\0' };
            bool sel = (kbdRow_ == r && kbdCol_ == cc);
            cell(startX + (uint16_t)cc * KEY_W, ry, KEY_W, lbl, sel);
        }
    }

    // Action row (row 4): 3 wide keys
    const uint16_t ry4 = startY + KBD_CHAR_ROWS * KEY_H;
    const int acol = snapActionCol(kbdCol_);
    const bool onAction = (kbdRow_ == KBD_CHAR_ROWS);

    // SPACE: cols 0-3 → 4 * KEY_W wide
    cell(startX,                   ry4, 4 * KEY_W, "SP",  onAction && acol == ACT_SP);
    // DEL:   cols 4-6 → 3 * KEY_W wide
    cell(startX + 4 * KEY_W,       ry4, 3 * KEY_W, "<",   onAction && acol == ACT_DEL);
    // ENT:   cols 7-9 → 3 * KEY_W wide
    cell(startX + 7 * KEY_W,       ry4, 3 * KEY_W, ">",   onAction && acol == ACT_ENT);
}

// ── onAction ─────────────────────────────────────────────────────────────────

bool FbconServer::onAction(input::Action action) {
    using input::Action;

    if (kbdOpen_) {
        switch (action) {
            // Up/Down = move between rows
            case Action::Prev:
                kbdRow_ = (kbdRow_ + KBD_CHAR_ROWS) % (KBD_CHAR_ROWS + 1);
                // clamp col when entering action row
                if (kbdRow_ == KBD_CHAR_ROWS) kbdCol_ = snapActionCol(kbdCol_);
                rt_.view().requestRedraw();
                return true;

            case Action::Next:
                kbdRow_ = (kbdRow_ + 1) % (KBD_CHAR_ROWS + 1);
                if (kbdRow_ == KBD_CHAR_ROWS) kbdCol_ = snapActionCol(kbdCol_);
                rt_.view().requestRedraw();
                return true;

            // Left/Right = move within a row
            case Action::AdjustDown:  // Left button → move left
                if (kbdRow_ == KBD_CHAR_ROWS) {
                    static const int SLOTS[] = { ACT_SP, ACT_DEL, ACT_ENT };
                    int s = snapActionCol(kbdCol_);
                    for (int i = 0; i < 3; i++)
                        if (SLOTS[i] == s) { kbdCol_ = SLOTS[(i + 2) % 3]; break; }
                } else {
                    kbdCol_ = (kbdCol_ + KBD_COLS - 1) % KBD_COLS;
                }
                rt_.view().requestRedraw();
                return true;

            case Action::AdjustUp:    // Right button → move right
                if (kbdRow_ == KBD_CHAR_ROWS) {
                    static const int SLOTS[] = { ACT_SP, ACT_DEL, ACT_ENT };
                    int s = snapActionCol(kbdCol_);
                    for (int i = 0; i < 3; i++)
                        if (SLOTS[i] == s) { kbdCol_ = SLOTS[(i + 1) % 3]; break; }
                } else {
                    kbdCol_ = (kbdCol_ + 1) % KBD_COLS;
                }
                rt_.view().requestRedraw();
                return true;

            // OK = type / submit / delete
            case Action::Activate:
                if (kbdRow_ < KBD_CHAR_ROWS) {
                    inputBuf_ += KBD_CHARS[kbdRow_][kbdCol_];
                } else {
                    int s = snapActionCol(kbdCol_);
                    if (s == ACT_SP)       inputBuf_ += ' ';
                    else if (s == ACT_DEL) { if (!inputBuf_.empty()) inputBuf_.pop_back(); }
                    else                   { kbdOpen_ = false; executeInput(); return true; }
                }
                rt_.view().requestRedraw();
                return true;

            // Back (double-tap OK) = close keyboard → history mode
            case Action::Back:
                kbdOpen_ = false;
                rt_.view().requestRedraw();
                return true;

            default:
                return true;
        }
    }

    // ── History mode (keyboard closed) ────────────────────────────────────────
    switch (action) {
        case Action::Activate:
            executeInput();
            rt_.view().requestRedraw();
            return true;
        case Action::Prev:
            histPrev();
            rt_.view().requestRedraw();
            return true;
        case Action::Next:
            histNext();
            rt_.view().requestRedraw();
            return true;
        case Action::Back:
            kbdOpen_ = true;
            rt_.view().requestRedraw();
            return true;
        default:
            return true;
    }
}

// ── helpers ───────────────────────────────────────────────────────────────────

void FbconServer::executeInput() {
    if (inputBuf_.empty()) return;
    outputLines_.push_back("> " + inputBuf_);
    CliService* svc = rt_.cliService();
    if (svc) svc->execute(inputBuf_, session_);
    else     outputLines_.push_back("cli: not available");
    inputBuf_.clear();
    histIdx_ = -1;
}

void FbconServer::histPrev() {
    const auto& hist = session_.history;
    if (hist.empty()) return;
    int next = histIdx_ + 1;
    if (next >= (int)hist.size()) return;
    histIdx_  = next;
    inputBuf_ = hist[hist.size() - 1 - (size_t)histIdx_];
}

void FbconServer::histNext() {
    if (histIdx_ <= 0) { histIdx_ = -1; inputBuf_.clear(); return; }
    histIdx_--;
    inputBuf_ = session_.history[session_.history.size() - 1 - (size_t)histIdx_];
}

} // namespace nema
