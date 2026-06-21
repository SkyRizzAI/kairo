#include "nema/screens/touch_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"

namespace nema {

using namespace aether::ui;

TouchSettingsScreen::TouchSettingsScreen(Runtime& rt) : ComponentScreen(rt) {}

void TouchSettingsScreen::onResume() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    rt_.view().requestRedraw();
}

UiNode* TouchSettingsScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    ListEntry e; e.label = "No touch settings yet";
    return View(a, root, {
        TitleBar(a, "Touch"),
        ListContainer(a, scroll_, {
            ListItemRow(a, e),
        }),
    });
}

} // namespace nema
