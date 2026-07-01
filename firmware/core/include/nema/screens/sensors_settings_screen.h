#pragma once
#include "nema/ui/component_screen.h"
#include <vector>
#include <string>

namespace nema {
class Runtime;

// Settings → Sensors. Lists every registered sensor (multi-instance) grouped by
// device, with each channel's live value + unit (Temp 24.3 C, Light 120 lx,
// X/Y/Z g …). Samples over I²C on a ~500 ms timer (not every frame). Gated on
// any sensors.* capability by the root Settings.
class SensorsSettingsScreen : public ComponentScreen {
public:
    explicit SensorsSettingsScreen(Runtime& rt);
    void        onResume() override;
    void        tick(uint64_t nowMs) override;   // throttled read + redraw
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    aether::ui::ScrollState  scroll_;
    std::vector<std::string> rows_;       // pre-formatted value strings (stable c_str)
    uint64_t                 lastRead_ = 0;
};

} // namespace nema
