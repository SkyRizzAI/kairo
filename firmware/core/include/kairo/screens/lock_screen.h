#pragma once
#include "kairo/ui/component_screen.h"

namespace kairo {

class Runtime;
class DisplayPowerManager;

// Lock screen — component-migrated (Plan 30), fullscreen. Press Activate twice
// to unlock. Custom onAction (it must not pop on Back like a normal screen).
class LockScreen : public ComponentScreen {
public:
    explicit LockScreen(Runtime& rt);

    void setDpm(DisplayPowerManager& dpm) { dpm_ = &dpm; }

    void        enter() override;
    void        onAction(input::Action a) override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

protected:
    bool fullscreen() const override { return true; }

private:
    DisplayPowerManager* dpm_         = nullptr;
    int                  selectCount_ = 0;
    bool                 hintVisible_ = false;
};

} // namespace kairo
