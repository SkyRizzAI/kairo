// HelloApp — canonical example of a ComponentApp using the Aether UI system.
// Plan 60 original; Plan 90 F6.28 modern widget API; now matches the current list
// language (ListSection subheader instead of a TitleBar banner, Switch instead of the
// old Toggle) so it's an accurate reference for new apps.
//
// Demonstrates: ListContainer, ListSection, ListItemRow, SwitchRow.
#include "nema/apps/hello_app.h"
#include "nema/ui/widgets.h"
#include "nema/app/app_context.h"

namespace nema {

using namespace aether::ui;

static void onToggle(void* u) { static_cast<HelloApp*>(u)->flipToggle(); }

aether::ui::UiNode* HelloApp::build(NodeArena& a, AppContext&) {
    Style root;
    root.dir   = FlexDir::Col;
    root.flexGrow = 1;
    root.align = Align::Stretch;

    ListEntry settings; settings.label = "Settings"; settings.chevron = true;
    ListEntry about;    about.label    = "About";    about.value = "v1";

    // No TitleBar banner — the first ListSection names the screen, like Settings.
    return View(a, root, {
        ListContainer(a, scroll_, {
            ListSection(a, "Hello Palanu"),
            ListItemRow(a, settings),
            ListItemRow(a, about),
            SwitchRow(a, "Dark mode", toggleOn_, onToggle, this),
        }),
    });
}

} // namespace nema
