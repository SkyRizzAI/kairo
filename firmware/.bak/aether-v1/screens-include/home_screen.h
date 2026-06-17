#pragma once
#include "nema/ui/component_screen.h"
#include "nema/screens/app_list_screen.h"
#include "nema/screens/logs_screen.h"
#include "nema/screens/settings_screen.h"

namespace nema {

class Runtime;

// Home — component-migrated (Plan 30). PALANU title + a tappable menu
// (Apps / Logs / Settings). Sub-screens are owned here and reused per visit.
class HomeScreen : public ComponentScreen {
public:
    explicit HomeScreen(Runtime& rt);
    void        enter() override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    AppListScreen  appList_;
    LogsScreen     logs_;
    SettingsScreen settings_;

    static void onApps(void* u);
    static void onLogs(void* u);
    static void onSettings(void* u);
    static void onContinue(void* u);   // Plan 22: resume paused app

    char continueLabel_[40] = "";      // "Continue: <app>" (stable for Text node)
};

} // namespace nema
