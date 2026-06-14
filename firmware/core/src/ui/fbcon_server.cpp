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

static constexpr const char* DEFAULT_CMD = "display start aether";

FbconServer::FbconServer(Runtime& rt) : rt_(rt) {
    inputBuf_ = DEFAULT_CMD;
    session_.id  = 0xFF;   // reserved id for the local console session
    session_.out = [this](const std::string& line) {
        outputLines_.push_back(line);
        rt_.view().requestRedraw();
    };
}

void FbconServer::renderFrame(Canvas& c, ViewDispatcher&, const StatusBarData&) {
    c.clear();

    const uint16_t lh = ui::CHAR_H + 1;
    if (c.height() < lh * 2) { c.flush(); return; }

    // Prompt is pinned to the bottom; banner to the top.
    const uint16_t promptY = c.height() - lh;
    const uint16_t bannerY = 1;

    // ── Banner ────────────────────────────────────────────────────────────────
    const SystemInfo& info = rt_.info();
    c.drawText(1, bannerY,        (info.boardName + "  v" + info.firmwareVersion).c_str(), true);
    c.drawText(1, bannerY + lh,   info.platformName.c_str(), true);

    // ── Output lines (most recent, filling upward from just above the prompt) ─
    const uint16_t outputTop = bannerY + lh * 2;
    if (promptY > outputTop && !outputLines_.empty()) {
        uint16_t slots = (promptY - outputTop) / lh;
        size_t total = outputLines_.size();
        size_t start = (total > slots) ? total - slots : 0;
        uint16_t y = outputTop;
        for (size_t i = start; i < total; i++) {
            c.drawText(1, y, outputLines_[i].c_str(), true);
            y += lh;
        }
    }

    // ── Prompt ────────────────────────────────────────────────────────────────
    c.drawText(1, promptY, ("> " + inputBuf_).c_str(), true);

    c.flush();
}

bool FbconServer::onAction(input::Action action) {
    using input::Action;
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
            inputBuf_.clear();
            histIdx_ = -1;
            rt_.view().requestRedraw();
            return true;
        default:
            return false;
    }
}

void FbconServer::executeInput() {
    if (inputBuf_.empty()) return;
    // Echo the submitted line so it appears in the output scroll.
    outputLines_.push_back("> " + inputBuf_);

    CliService* cli = rt_.cliService();
    if (cli) {
        cli->execute(inputBuf_, session_);
    } else {
        outputLines_.push_back("cli: not available");
    }

    inputBuf_.clear();
    histIdx_ = -1;
}

void FbconServer::histPrev() {
    const auto& hist = session_.history;
    if (hist.empty()) return;
    int next = histIdx_ + 1;
    if (next >= (int)hist.size()) return;  // already at oldest
    histIdx_ = next;
    inputBuf_ = hist[hist.size() - 1 - (size_t)histIdx_];
}

void FbconServer::histNext() {
    if (histIdx_ <= 0) {
        histIdx_ = -1;
        inputBuf_.clear();
        return;
    }
    histIdx_--;
    inputBuf_ = session_.history[session_.history.size() - 1 - (size_t)histIdx_];
}

} // namespace nema
