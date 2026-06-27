#pragma once
#include "nema/ui/component_screen.h"
#include <vector>
#include <string>

namespace nema {
class Runtime;

// Sounds — component-migrated (Plan 30). Live input/output level meters (text
// bars, refreshed each tick) + a Test Beep row. Scrollable.
class SoundsSettingsScreen : public ComponentScreen {
public:
    explicit SoundsSettingsScreen(Runtime& rt);
    void        onResume() override;
    void        tick(uint64_t nowMs) override;   // live meter refresh
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    aether::ui::ScrollState          scroll_;
    std::vector<std::string> rows_;
};

} // namespace nema
