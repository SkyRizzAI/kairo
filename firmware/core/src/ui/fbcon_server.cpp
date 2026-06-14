#include "nema/ui/fbcon_server.h"
#include "nema/ui/canvas.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/runtime.h"
#include "nema/system/system_info.h"
#include "nema/services/cli_service.h"
#include "nema/input/input_action.h"
#include <string>

namespace nema {

// ── Keyboard layout ───────────────────────────────────────────────────────────
// 3 rows × 13 keys. Special: '<' = backspace, '>' = submit, ' ' = space.
static constexpr int  KBD_ROWS = 3;
static constexpr int  KBD_COLS = 13;
static constexpr const char* KBD[KBD_ROWS] = {
    "abcdefghijklm",
    "nopqrstuvwxyz",
    "0123456789 ><",   // space, backspace (<), enter (>)
};
// Each key cell: 1px padding on each side of the 6px glyph = 8px wide,
//                1px padding on each side of the 9px glyph = 11px tall.
static constexpr uint16_t KEY_W = ui::CHAR_W + 2;   // 8 px
static constexpr uint16_t KEY_H = ui::CHAR_H + 2;   // 11 px
static constexpr uint16_t KBD_W = KBD_COLS * KEY_W; // 104 px
static constexpr uint16_t KBD_H = KBD_ROWS * KEY_H; // 33 px

static constexpr const char* DEFAULT_CMD = "display start aether";

// ── Constructor ───────────────────────────────────────────────────────────────

FbconServer::FbconServer(Runtime& rt) : rt_(rt) {
    inputBuf_ = DEFAULT_CMD;
    session_.id  = 0xFF;
    session_.out = [this](const std::string& line) {
        outputLines_.push_back(line);
        rt_.view().requestRedraw();
    };
}

// ── renderFrame ───────────────────────────────────────────────────────────────

void FbconServer::renderFrame(Canvas& c, ViewDispatcher&, const StatusBarData&) {
    c.clear();

    const uint16_t lh = ui::CHAR_H + 1;   // line height (10 px)
    if (c.height() < lh * 2) { c.flush(); return; }

    // When the keyboard is open it sits at the very bottom; the prompt lives
    // just above it. When closed the prompt is at the bottom.
    const uint16_t kbdH    = kbdOpen_ ? KBD_H : 0;
    const uint16_t promptY = c.height() - lh - kbdH;

    // ── Banner ────────────────────────────────────────────────────────────────
    const SystemInfo& info = rt_.info();
    c.drawText(1, 1,      (info.boardName + "  v" + info.firmwareVersion).c_str(), true);
    c.drawText(1, 1 + lh, info.platformName.c_str(), true);

    // ── Output lines (fill downward from below the banner, clipped above prompt)
    const uint16_t outputTop = 1 + lh * 2;
    if (promptY > outputTop && !outputLines_.empty()) {
        uint16_t slots = (promptY - outputTop) / lh;
        size_t total   = outputLines_.size();
        size_t start   = (total > slots) ? total - slots : 0;
        uint16_t y     = outputTop;
        for (size_t i = start; i < total; i++) {
            c.drawText(1, y, outputLines_[i].c_str(), true);
            y += lh;
        }
    }

    // ── Prompt ────────────────────────────────────────────────────────────────
    c.drawText(1, promptY, ("> " + inputBuf_).c_str(), true);

    // ── Virtual keyboard (floating, center-bottom) ────────────────────────────
    if (kbdOpen_) drawKeyboard(c);

    c.flush();
}

// ── drawKeyboard ──────────────────────────────────────────────────────────────

void FbconServer::drawKeyboard(Canvas& c) const {
    // Center horizontally; pin to the very bottom of the canvas.
    const uint16_t kx = (c.width() > KBD_W) ? (c.width() - KBD_W) / 2 : 0;
    const uint16_t ky = (c.height() >= KBD_H) ? c.height() - KBD_H : 0;

    for (int row = 0; row < KBD_ROWS; row++) {
        for (int col = 0; col < KBD_COLS; col++) {
            char ch   = KBD[row][col];
            char disp = (ch == ' ') ? '_' : ch;  // show underscore for space
            char text[2] = { disp, '\0' };

            uint16_t cellX = kx + (uint16_t)col * KEY_W;
            uint16_t cellY = ky + (uint16_t)row * KEY_H;
            uint16_t tx    = cellX + 1;  // 1px inner left padding
            uint16_t ty    = cellY + 1;  // 1px inner top padding

            if (row == kbdRow_ && col == kbdCol_) {
                // Selected key: white block with dark glyph
                c.fillRect(cellX, cellY, KEY_W, KEY_H, true);
                c.drawText(tx, ty, text, false);
            } else {
                c.drawText(tx, ty, text, true);
            }
        }
    }
}

// ── onAction ─────────────────────────────────────────────────────────────────

bool FbconServer::onAction(input::Action action) {
    using input::Action;

    if (kbdOpen_) {
        switch (action) {
            case Action::Prev:
                if (--kbdCol_ < 0) {
                    kbdRow_ = (kbdRow_ + KBD_ROWS - 1) % KBD_ROWS;
                    kbdCol_ = KBD_COLS - 1;
                }
                rt_.view().requestRedraw();
                return true;

            case Action::Next:
                if (++kbdCol_ >= KBD_COLS) {
                    kbdRow_ = (kbdRow_ + 1) % KBD_ROWS;
                    kbdCol_ = 0;
                }
                rt_.view().requestRedraw();
                return true;

            case Action::Activate: {
                char ch = KBD[kbdRow_][kbdCol_];
                if (ch == '>') {           // Enter key
                    kbdOpen_ = false;
                    executeInput();
                } else if (ch == '<') {   // Backspace key
                    if (!inputBuf_.empty()) inputBuf_.pop_back();
                    rt_.view().requestRedraw();
                } else {
                    inputBuf_ += ch;      // letter, digit, or space
                    rt_.view().requestRedraw();
                }
                return true;
            }

            case Action::Back:
                kbdOpen_ = false;
                rt_.view().requestRedraw();
                return true;

            default:
                return false;
        }
    }

    // ── Normal mode (keyboard closed) ─────────────────────────────────────────
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
            kbdRow_  = 0;
            kbdCol_  = 0;
            rt_.view().requestRedraw();
            return true;

        default:
            return false;
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
