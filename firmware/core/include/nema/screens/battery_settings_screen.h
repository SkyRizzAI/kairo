#pragma once
#include "nema/ui/component_screen.h"
#include <vector>
#include <string>

namespace nema {
class Runtime;

// Settings → Battery. Live charge level + charging state, read from the
// IBatteryDriver in the container. Gated on caps::Battery by the root Settings.
class BatterySettingsScreen : public ComponentScreen {
public:
    explicit BatterySettingsScreen(Runtime& rt);
    void        onResume() override;
    void        tick(uint64_t nowMs) override;   // periodic refresh
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    aether::ui::ScrollState  scroll_;
    std::vector<std::string> rows_;
    uint64_t                 lastRedraw_ = 0;
};

} // namespace nema
