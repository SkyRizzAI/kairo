#include "kairo/screens/lock_screen.h"
#include "kairo/runtime.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/services/display_power_manager.h"

namespace kairo {

using namespace ui;

LockScreen::LockScreen(Runtime& rt) : ComponentScreen(rt, 16) {}

void LockScreen::enter() {
    selectCount_ = 0;
    hintVisible_ = false;
    rt_.view().requestRedraw();
}

void LockScreen::onAction(input::Action a) {
    hintVisible_ = true;
    if (a == input::Action::Activate) {
        if (++selectCount_ >= 2 && dpm_) dpm_->unlock();
    } else {
        selectCount_ = 0;   // any other press resets the unlock sequence
    }
    rt_.view().requestRedraw();
}

UiNode* LockScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 4;
    root.align = Align::Center; root.justify = Justify::Center; root.gap = 8;
    return View(a, root, {
        Text(a, "LOCKED", TextRole::Title),
        hintVisible_ ? Text(a, "Activate x2 to unlock", TextRole::Caption) : nullptr,
    });
}

} // namespace kairo
