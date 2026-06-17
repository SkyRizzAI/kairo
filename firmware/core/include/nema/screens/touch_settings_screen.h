#pragma once
#include "nema/ui/component_screen.h"

namespace nema {

class Runtime;

// Settings → Touch. Plan 60 cleanup — TouchTestApp removed. Shows touch status.
class TouchSettingsScreen : public ComponentScreen {
public:
    explicit TouchSettingsScreen(Runtime& rt);
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;
};

} // namespace nema
