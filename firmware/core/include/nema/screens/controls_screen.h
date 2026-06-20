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
    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    aether::ui::ScrollState          scroll_;
    std::vector<std::string> rows_;
};

} // namespace nema
