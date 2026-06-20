#pragma once
#include "nema/ui/component_screen.h"

namespace nema {

class Runtime;
class DisplayPowerManager;

// Lock screen — component-migrated (Plan 30), fullscreen. Press Activate twice
// to unlock. Custom onAction (it must not pop on Back like a normal screen).
class LockScreen : public ComponentScreen {
public:
    explicit LockScreen(Runtime& rt);

    void setDpm(DisplayPowerManager& dpm) { dpm_ = &dpm; }

    void        onResume() override;
    void        onAction(input::Action a) override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

protected:
    bool fullscreen() const override { return true; }

private:
    DisplayPowerManager* dpm_         = nullptr;
    int                  selectCount_ = 0;
    bool                 hintVisible_ = false;
    char                 clock_[8]    = "";   // "HH:MM", stable for the Text node
    char                 hint_[48]    = "";   // "Double-tap <btn> to unlock"
};

} // namespace nema
