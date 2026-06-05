#pragma once
#include "kairo/ui/component_screen.h"
#include <vector>
#include <string>

namespace kairo {
class Runtime;

// Sounds — component-migrated (Plan 30). Live input/output level meters (text
// bars, refreshed each tick) + a Test Beep row. Scrollable.
class SoundsSettingsScreen : public ComponentScreen {
public:
    explicit SoundsSettingsScreen(Runtime& rt);
    void        enter() override;
    void        tick(uint64_t nowMs) override;   // live meter refresh
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    ui::ScrollState          scroll_;
    std::vector<std::string> rows_;

    static void onTestBeep(void* u);
};

} // namespace kairo
