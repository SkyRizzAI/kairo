#include "kairo/screens/home_screen.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/runtime.h"

namespace kairo {

using namespace ui;

HomeScreen::HomeScreen(Runtime& rt)
    : ComponentScreen(rt), appList_(rt), logs_(rt), settings_(rt) {}

void HomeScreen::enter() { rt_.view().requestRedraw(); }

void HomeScreen::onApps(void* u)     { auto* s = static_cast<HomeScreen*>(u); s->rt_.view().push(s->appList_); }
void HomeScreen::onLogs(void* u)     { auto* s = static_cast<HomeScreen*>(u); s->rt_.view().push(s->logs_); }
void HomeScreen::onSettings(void* u) { auto* s = static_cast<HomeScreen*>(u); s->rt_.view().push(s->settings_); }

UiNode* HomeScreen::build(NodeArena& a, Runtime&) {
    Style root;  root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 4; root.gap = 6;
    root.align = Align::Stretch;

    Style titleRow; titleRow.dir = FlexDir::Row; titleRow.justify = Justify::Center;
    Style menu; menu.dir = FlexDir::Col; menu.align = Align::Stretch; menu.gap = 3;
    menu.flexGrow = 1; menu.justify = Justify::Center;

    return View(a, root, {
        Row(a, titleRow, { Text(a, "KAIRO", TextRole::Title) }),
        Col(a, menu, {
            ListRow(a, "Apps",     onApps,     this),
            ListRow(a, "Logs",     onLogs,     this),
            ListRow(a, "Settings", onSettings, this),
        }),
    });
}

} // namespace kairo
