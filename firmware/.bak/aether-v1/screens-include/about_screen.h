#pragma once
#include "nema/ui/component_screen.h"
#include <vector>
#include <string>

namespace nema {

class Runtime;

// About — component-migrated (Plan 30). Header + a scrollable list of system
// info / capability rows.
class AboutScreen : public ComponentScreen {
public:
    explicit AboutScreen(Runtime& rt);
    void        enter() override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    ui::ScrollState          scroll_;
    std::vector<std::string> rows_;   // backing storage for Text node strings
};

} // namespace nema
