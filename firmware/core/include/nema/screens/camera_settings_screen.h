#pragma once
#include "nema/ui/component_screen.h"
#include <vector>
#include <string>

namespace nema {

class Runtime;

// Camera settings — component-migrated (Plan 30). Header + scrollable list of
// camera devices with their resolution.
class CameraSettingsScreen : public ComponentScreen {
public:
    explicit CameraSettingsScreen(Runtime& rt);
    void        onResume() override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    ui::ScrollState          scroll_;
    std::vector<std::string> rows_;
};

} // namespace nema
