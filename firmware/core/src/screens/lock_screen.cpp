#include "nema/screens/lock_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/services/display_power_manager.h"

namespace nema {

using namespace ui;

LockScreen::LockScreen(Runtime& rt) : ComponentScreen(rt, 16) {}

void LockScreen::enter() {
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

UiNode* LockScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 4;
    root.align = Align::Center; root.justify = Justify::Center; root.gap = 8;
    return View(a, root, {
        Text(a, "LOCKED", TextRole::Title),
        hintVisible_ ? Text(a, "Double-tap OK to unlock", TextRole::Caption) : nullptr,
    });
}

} // namespace nema
