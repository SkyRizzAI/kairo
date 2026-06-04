#include "kairo/screens/lock_screen.h"
#include "kairo/services/display_power_manager.h"
#include "kairo/ui/canvas.h"

namespace kairo {

void LockScreen::enter() {
    selectCount_ = 0;
    hintVisible_ = false;
}

void LockScreen::update(Key k) {
    hintVisible_ = true;
    if (k == Key::Select) {
        if (++selectCount_ >= 2 && dpm_) dpm_->unlock();
    } else {
        selectCount_ = 0;
    }
}

void LockScreen::draw(Canvas& canvas) {
    canvas.clear(false);
    if (hintVisible_)
        canvas.drawText(4, canvas.height() - 12, "Press Select 2x to unlock");
}

} // namespace kairo
