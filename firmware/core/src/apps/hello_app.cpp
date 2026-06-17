// HelloApp — Plan 60 demo. Shows TitleBar, SmartLabel, ListItem, Toggle.
// Proves the new widget system compiles and renders correctly.
#include "nema/apps/hello_app.h"
#include "nema/ui/widgets.h"
#include "nema/ui/style_tokens.h"
#include "nema/app/app_context.h"

namespace nema {

using namespace ui;

static void onToggle(void* u) { static_cast<HelloApp*>(u)->flipToggle(); }

ui::UiNode* HelloApp::build(NodeArena& arena, AppContext&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1;
    root.padding = theme().space.sm; root.gap = theme().space.sm;
    root.align = Align::Stretch;

    Style menu; menu.dir = FlexDir::Col; menu.align = Align::Stretch; menu.gap = 1;

    return View(arena, root, {
        TitleBar(arena, "HELLO"),
        Col(arena, menu, {
            SmartLabel(arena, "Welcome to Aether UI"),
            ListItem(arena, "Settings", ">",  nullptr, nullptr),
            ListItem(arena, "About",    "v1", nullptr, nullptr),
            Toggle(arena, "Dark mode",  toggleOn_, onToggle, this),
        }),
    });
}


} // namespace nema
