#pragma once
#include "nema/ui/component_screen.h"
#include <vector>
#include <string>

namespace nema {
class Runtime;

// Settings → LEDs. Lists every registered LED (multi-instance, like Sounds) with
// its pixel count + colour model, plus test actions: solid colours, blink, the
// board-agnostic notification intents, and a brightness stepper. Gated on the
// Led/Rgb capability by the root Settings.
class LedSettingsScreen : public ComponentScreen {
public:
    explicit LedSettingsScreen(Runtime& rt);
    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    aether::ui::ScrollState  scroll_;
    std::vector<std::string> rows_;      // pre-formatted value strings (stable c_str)
    int                      brightness_ = 255;
};

} // namespace nema
