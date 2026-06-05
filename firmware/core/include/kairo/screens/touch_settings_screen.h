#pragma once
#include "kairo/ui/component_screen.h"
#include "kairo/apps/touch_test_app.h"
#include <memory>

namespace kairo {

class Runtime;
class AppHost;

// Settings → Touch. Component-migrated (Plan 30). Launches the Touch Test
// diagnostic app; room for calibration / sensitivity later.
class TouchSettingsScreen : public ComponentScreen {
public:
    explicit TouchSettingsScreen(Runtime& rt);
    ~TouchSettingsScreen() override;   // out-of-line: AppHost incomplete here
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    TouchTestApp             touchApp_;
    std::unique_ptr<AppHost> touchHost_;

    static void onTouchTest(void* u);
};

} // namespace kairo
