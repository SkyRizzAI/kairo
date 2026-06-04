#pragma once
#include "kairo/ui/screen.h"
#include "kairo/ui/key.h"

namespace kairo {

class DisplayPowerManager;

class LockScreen : public IScreen {
public:
    void setDpm(DisplayPowerManager& dpm) { dpm_ = &dpm; }

    ScreenMode mode() const override { return ScreenMode::Fullscreen; }
    void enter()  override;
    void update(Key k) override;
    void draw(Canvas& canvas) override;

private:
    DisplayPowerManager* dpm_         = nullptr;
    int                  selectCount_ = 0;
    bool                 hintVisible_ = false;
};

} // namespace kairo
