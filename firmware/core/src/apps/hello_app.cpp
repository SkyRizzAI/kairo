// HelloApp — canonical example of a ComponentApp using the Aether UI system.
// Plan 60 original; Plan 90 F6.28 updated to modern widget API.
//
// Demonstrates: ListContainer, ListSection, ListItemRow, Toggle, TitleBar.
// Use this as the reference when building a new app with the component system.
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

    return View(a, root, {
        TitleBar(a, "HELLO PALANU"),
        ListContainer(a, scroll_, {
            ListSection(a, "Menu"),
            ListItemRow(a, settings),
            ListItemRow(a, about),
            Toggle(a, "Dark mode", toggleOn_, onToggle, this),
        }),
    });
}

} // namespace nema
