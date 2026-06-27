#include "nema/screens/touch_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"

namespace nema {

using namespace aether::ui;

TouchSettingsScreen::TouchSettingsScreen(Runtime& rt) : ComponentScreen(rt) {}

void TouchSettingsScreen::onResume() {
    rt_.view().requestRedraw();
}

UiNode* TouchSettingsScreen::build(NodeArena& a, Runtime&) {
    MenuBuilder m(a, scroll_, this);
    m.info("No touch settings yet", nullptr);
    return m.build();
}

} // namespace nema
