#pragma once
#include "nema/ui/component_screen.h"
#include "nema/apps/touch_test_app.h"

namespace nema {

class Runtime;

// Settings → Touch. Offers the fullscreen Touch Test diagnostic (marker follows
// your finger + live coords). Gated behind caps::InputTouch by the root Settings.
class TouchSettingsScreen : public ComponentScreen {
public:
    explicit TouchSettingsScreen(Runtime& rt);
    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    aether::ui::ScrollState scroll_;
    TouchTestApp            touchApp_;   // launched via appHost() (not in launcher)
};

} // namespace nema
