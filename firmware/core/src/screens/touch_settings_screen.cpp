#include "kairo/screens/touch_settings_screen.h"
#include "kairo/runtime.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/app/app_host.h"

namespace kairo {

using namespace ui;

TouchSettingsScreen::TouchSettingsScreen(Runtime& rt) : ComponentScreen(rt) {}
TouchSettingsScreen::~TouchSettingsScreen() = default;

void TouchSettingsScreen::onTouchTest(void* u) {
    auto* s = static_cast<TouchSettingsScreen*>(u);
    s->touchHost_ = std::make_unique<AppHost>(s->rt_, s->touchApp_);
    s->rt_.view().push(*s->touchHost_);
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

} // namespace kairo
