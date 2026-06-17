#include "nema/apps/stopwatch_app.h"
#include "nema/app/app_context.h"
#include "nema/runtime.h"
#include "nema/clock.h"
#include "nema/ui/widgets.h"
#include <cstdio>

namespace nema {

using namespace ui;

uint64_t StopwatchApp::totalMs(AppContext& ctx) const {
    if (!running_) return elapsed_;
    return elapsed_ + (ctx.runtime().clock().millis() - startMs_);
}

bool StopwatchApp::onKey(Key k, AppContext& ctx) {
    if (k == Key::Select) {
        if (running_) { elapsed_ += ctx.runtime().clock().millis() - startMs_; running_ = false; }
        else          { startMs_ = ctx.runtime().clock().millis();             running_ = true; }
        return true;
    }
    if (k == Key::Up) {
        if (!running_) elapsed_ = 0;   // reset when stopped
        return true;
    }
    return false;   // Cancel → base exits
}

UiNode* StopwatchApp::build(NodeArena& a, AppContext& ctx) {
    uint64_t total = totalMs(ctx);
    uint32_t ms = (uint32_t)(total % 1000);
    uint32_t s  = (uint32_t)(total / 1000) % 60;
    uint32_t m  = (uint32_t)(total / 60000) % 60;
    uint32_t h  = (uint32_t)(total / 3600000);
    if (h > 0)
        std::snprintf(timeBuf_, sizeof(timeBuf_), "%02u:%02u:%02u.%02u",
                      (unsigned)h, (unsigned)m, (unsigned)s, (unsigned)(ms / 10));
    else
        std::snprintf(timeBuf_, sizeof(timeBuf_), "%02u:%02u.%03u",
                      (unsigned)m, (unsigned)s, (unsigned)ms);

    Style root;
    root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 4; root.gap = 6;
    root.align = Align::Center; root.justify = Justify::Center;

    return View(a, root, {
        Text(a, timeBuf_, TextRole::Title),
        Text(a, running_ ? "[ RUNNING ]" : "[ STOPPED ]", TextRole::Body),
        Text(a, "Select=start/stop  Up=reset", TextRole::Caption),
    });
}

} // namespace nema
