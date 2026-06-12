#pragma once
#include "nema/ui/component_screen.h"
#include <vector>
#include <string>

namespace nema {

class Runtime;

// Read-only introspection of the input system: board name, button count,
// Action → hint mapping, and gesture timings. Component-migrated (Plan 30):
// header + a scrollable list. Accessed via Settings → Controls.
class ControlsScreen : public ComponentScreen {
public:
    explicit ControlsScreen(Runtime& rt);
    void        enter() override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    ui::ScrollState          scroll_;
    std::vector<std::string> rows_;
};

} // namespace nema
