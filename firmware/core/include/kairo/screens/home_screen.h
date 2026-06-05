#pragma once
#include "kairo/ui/component_screen.h"
#include "kairo/screens/app_list_screen.h"
#include "kairo/screens/logs_screen.h"
#include "kairo/screens/settings_screen.h"

namespace kairo {

class Runtime;

// Home — component-migrated (Plan 30). KAIRO title + a tappable menu
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
};

} // namespace kairo
