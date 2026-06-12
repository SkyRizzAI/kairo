#include "nema/screens/home_screen.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/app/app_host_manager.h"
#include "nema/runtime.h"
#include <cstdio>

namespace nema {

using namespace ui;

HomeScreen::HomeScreen(Runtime& rt)
    : ComponentScreen(rt), appList_(rt), logs_(rt), settings_(rt) {}

void HomeScreen::enter() { ComponentScreen::enter(); }

void HomeScreen::onApps(void* u)     { auto* s = static_cast<HomeScreen*>(u); s->rt_.view().push(s->appList_); }
void HomeScreen::onLogs(void* u)     { auto* s = static_cast<HomeScreen*>(u); s->rt_.view().push(s->logs_); }
void HomeScreen::onSettings(void* u) { auto* s = static_cast<HomeScreen*>(u); s->rt_.view().push(s->settings_); }
void HomeScreen::onContinue(void* u) { static_cast<HomeScreen*>(u)->rt_.appHost().resumePaused(); }

UiNode* HomeScreen::build(NodeArena& a, Runtime& rt) {
    Style root;  root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 4; root.gap = 6;
    root.align = Align::Stretch;

    Style titleRow; titleRow.dir = FlexDir::Row; titleRow.justify = Justify::Center;
    Style menu; menu.dir = FlexDir::Col; menu.align = Align::Stretch; menu.gap = 3;
    menu.flexGrow = 1; menu.justify = Justify::Center;

    UiNode* menuNode = Col(a, menu, {});
    UiNode* prev = nullptr;
    auto add = [&](UiNode* n) {
        if (!n) return;
        if (!prev) menuNode->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };
    // Plan 22: a paused app appears as the top "Continue" entry.
    if (rt.appHost().hasPaused()) {
        std::snprintf(continueLabel_, sizeof(continueLabel_), "Continue: %s",
                      rt.appHost().pausedName() ? rt.appHost().pausedName() : "app");
        add(ListRow(a, continueLabel_, onContinue, this));
    }
    add(ListRow(a, "Apps",     onApps,     this));
    add(ListRow(a, "Logs",     onLogs,     this));
    add(ListRow(a, "Settings", onSettings, this));

    return View(a, root, {
        Row(a, titleRow, { Text(a, "PALANU", TextRole::Title) }),
        menuNode,
    });
}

} // namespace nema
