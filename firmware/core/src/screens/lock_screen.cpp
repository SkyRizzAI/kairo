// Plan 60 — LockScreen: themed big clock + unlock hint.
#include "nema/screens/lock_screen.h"
#include "nema/runtime.h"
#include "nema/clock.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/style_tokens.h"
#include "nema/services/display_power_manager.h"
#include <cstdio>
#include <ctime>

namespace nema {

using namespace ui;

LockScreen::LockScreen(Runtime& rt) : ComponentScreen(rt, 16) {}

void LockScreen::onResume() {
    selectCount_ = 0;
    hintVisible_ = false;
    rt_.view().requestRedraw();
}

void LockScreen::onAction(input::Action a) {
    hintVisible_ = true;
    // Unlock on a fast double-tap (= Back on boards where double-tap maps to Back,
    // e.g. skyrizz middle) OR two single Activates (slow taps / other boards). This
    // avoids the trap where a quick double-OK is swallowed as one Back.
    if (a == input::Action::Back) {
        if (dpm_) dpm_->unlock();
    } else if (a == input::Action::Activate) {
        if (++selectCount_ >= 2 && dpm_) dpm_->unlock();
    } else {
        selectCount_ = 0;   // any other press resets the slow-tap sequence
    }
    rt_.view().requestRedraw();
}

UiNode* LockScreen::build(NodeArena& a, Runtime& rt) {
    // Wall-clock HH:MM (falls back to "--:--" before NTP/RTC sync).
    time_t t = (time_t)(rt.clock().epochMs() / 1000);
    struct tm* tm = localtime(&t);
    if (tm) std::snprintf(clock_, sizeof(clock_), "%02d:%02d", tm->tm_hour, tm->tm_min);
    else    std::snprintf(clock_, sizeof(clock_), "--:--");

    // Unlock hint uses the board's own button label (never hardcode names).
    const char* okBtn = rt.input().hintFor(input::Action::Activate);
    std::snprintf(hint_, sizeof(hint_), "Double-tap %s to unlock",
                  (okBtn && *okBtn) ? okBtn : "OK");

    uint8_t gap = nema::theme().space.lg;
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = nema::theme().space.md;
    root.align = Align::Center; root.justify = Justify::Center; root.gap = gap;
    return View(a, root, {
        Text(a, clock_, TextRole::Title),
        Text(a, "LOCKED", TextRole::Caption),
        hintVisible_ ? Text(a, hint_, TextRole::Caption) : nullptr,
    });
}

} // namespace nema
