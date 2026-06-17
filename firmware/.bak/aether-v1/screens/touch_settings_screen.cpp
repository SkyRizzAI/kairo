#include "nema/screens/touch_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/app/app_host_manager.h"

namespace nema {

using namespace ui;

TouchSettingsScreen::TouchSettingsScreen(Runtime& rt) : ComponentScreen(rt) {}

void TouchSettingsScreen::onTouchTest(void* u) {
    auto* s = static_cast<TouchSettingsScreen*>(u);
    s->rt_.appHost().launch(s->touchApp_);
}

UiNode* TouchSettingsScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 3; root.gap = 1;
    Style line; line.height = 1; line.background = true;
    Style menu; menu.dir = FlexDir::Col; menu.align = Align::Stretch; menu.gap = 2;

    return View(a, root, {
        Text(a, "TOUCH", TextRole::Title),
        View(a, line, {}),
        Col(a, menu, {
            ListRow(a, "Touch Test", onTouchTest, this),
        }),
    });
}

} // namespace nema
