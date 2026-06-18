#pragma once
#include "nema/ui/component_screen.h"
#include <vector>
#include <string>
#include <memory>

namespace nema {

class Runtime;
class ComponentScreen; // Plan 70: AboutModal is defined in about_screen.cpp

// About — component-migrated (Plan 30). Header + a scrollable list of system
// info / capability rows. Plan 70: Activate pushes an AboutModal dialog.
class AboutScreen : public ComponentScreen {
public:
    explicit AboutScreen(Runtime& rt);
    void        onResume() override;
    void        onAction(input::Action a) override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    ui::ScrollState          scroll_;
    std::vector<std::string> rows_;   // backing storage for Text node strings

    // Plan 70: modal dialog — owned here, defined in .cpp
    std::unique_ptr<ComponentScreen> aboutModal_;
};

} // namespace nema
