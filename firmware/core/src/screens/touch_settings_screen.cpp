#include "nema/screens/touch_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/app/app_host_manager.h"

namespace nema {

using namespace aether::ui;

TouchSettingsScreen::TouchSettingsScreen(Runtime& rt) : ComponentScreen(rt) {}

void TouchSettingsScreen::onResume() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    rt_.view().requestRedraw();
}

#define S(u) static_cast<TouchSettingsScreen*>(u)

UiNode* TouchSettingsScreen::build(NodeArena& a, Runtime&) {
    MenuBuilder m(a, scroll_, this);
    m.section("Touch");
    m.nav("Touch Test", [](void* u){ S(u)->rt_.appHost().launch(S(u)->touchApp_); });
    return m.build();
}

#undef S

} // namespace nema
