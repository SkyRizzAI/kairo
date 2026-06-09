#pragma once
#include "kairo/ui/component_screen.h"
#include "kairo/apps/touch_test_app.h"

namespace kairo {

class Runtime;

// Settings → Touch. Component-migrated (Plan 30). Launches the Touch Test
// diagnostic app via AppHostManager; room for calibration / sensitivity later.
class TouchSettingsScreen : public ComponentScreen {
public:
    explicit TouchSettingsScreen(Runtime& rt);
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    TouchTestApp touchApp_;

    static void onTouchTest(void* u);
};

} // namespace kairo
