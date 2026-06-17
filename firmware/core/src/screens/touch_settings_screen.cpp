#include "nema/screens/touch_settings_screen.h"
#include "nema/runtime.h"

namespace nema {

using namespace ui;

TouchSettingsScreen::TouchSettingsScreen(Runtime& rt) : ComponentScreen(rt) {}

UiNode* TouchSettingsScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 3; root.gap = 1;
    root.align = Align::Stretch;

    return View(a, root, {
        TitleBar(a, "TOUCH"),
        Text(a, "Touch settings", TextRole::Body),
    });
}

} // namespace nema
