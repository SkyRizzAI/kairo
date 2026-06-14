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
    const uint16_t maxLines = c.height() / lh;
    if (maxLines == 0) { c.flush(); return; }

    // Reserve bottom line for the prompt; banner takes 2 lines at most.
    // Available lines for output = maxLines - 1 (prompt) - 2 (banner).
    const uint16_t bannerLines = 2;
    const uint16_t promptLines = 1;
    const uint16_t outputSlots = (maxLines > bannerLines + promptLines)
                                 ? maxLines - bannerLines - promptLines : 0;

    uint16_t y = 1;
    auto drawLine = [&](const std::string& s) {
        if (y + lh > c.height()) return;
        c.drawText(1, y, s.c_str(), true);
        y += lh;
    };

    // ── Banner ────────────────────────────────────────────────────────────────
    const SystemInfo& info = rt_.info();
    drawLine(info.boardName + "  v" + info.firmwareVersion);
    drawLine(info.platformName);

    // ── Output lines (most recent, bottom-aligned within the slot) ────────────
    if (outputSlots > 0) {
        size_t total = outputLines_.size();
        size_t start = (total > outputSlots) ? total - outputSlots : 0;
        for (size_t i = start; i < total; i++)
            drawLine(outputLines_[i]);
        // Fill any empty slots so the prompt stays anchored at the bottom.
        uint16_t used = (uint16_t)(total - start);
        for (uint16_t i = used; i < outputSlots; i++) y += lh;
    }

    // ── Prompt ────────────────────────────────────────────────────────────────
    drawLine("> " + inputBuf_);

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
