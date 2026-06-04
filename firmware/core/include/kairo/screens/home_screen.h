#pragma once
#include "kairo/ui/screen.h"
#include "kairo/screens/app_list_screen.h"
#include "kairo/screens/logs_screen.h"
#include "kairo/screens/settings_screen.h"
#include <cstdint>

namespace kairo {

class Runtime;

class HomeScreen : public IScreen {
public:
    explicit HomeScreen(Runtime& rt);

    void enter() override;
    void update(Key key) override;
    void draw(Canvas& c) override;
    void tick(uint64_t nowMs) override;

private:
    Runtime& rt_;
    int      cursor_ = 0;

    // Sub-screens owned here — constructed once, reused on each visit
    AppListScreen  appList_;
    LogsScreen     logs_;
    SettingsScreen settings_;

    static constexpr int MENU_SIZE = 3;
    static const char*   MENU_LABELS[MENU_SIZE];

    void drawMenu(Canvas& c);
};

} // namespace kairo
