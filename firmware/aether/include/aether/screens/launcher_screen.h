#pragma once
// Plan 81 — LauncherScreen: the system menu opened from the Desktop.
//
// Owns ALL behaviour: the fixed system entry model, the cursor, linear
// navigation, Activate routing, and Back→Desktop. The visual skin is swappable
// (PlayStation carousel ↔ Wii grid) via ILauncherTheme, selected from config.
// Apps are NOT listed here — "Apps" is one entry that opens AppListScreen.
#include "nema/ui/component_screen.h"
#include "aether/shell/launcher_theme.h"
#include "nema/ui/animation_player.h"
#include "nema/screens/app_list_screen.h"
#include "nema/screens/file_browser_screen.h"
#include "nema/screens/dolphin_demo.h"
#include "nema/screens/logs_screen.h"
#include "nema/screens/settings_screen.h"
#include "nema/apps/bad_usb_app.h"
#include <memory>
#include <vector>

namespace nema {

class Runtime;

class LauncherScreen : public ComponentScreen {
public:
    explicit LauncherScreen(Runtime& rt);

    void        onResume() override;
    void        draw(Canvas& c) override;
    void        tick(uint64_t nowMs) override;
    void        onAction(input::Action a) override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    // Sub-screens reachable from the launcher (owned here, like the old HomeScreen).
    AppListScreen     appList_;
    FileBrowserScreen files_;
    DolphinDemoScreen dolphin_;
    LogsScreen        logs_;
    SettingsScreen    settings_;
    BadUsbApp         badUsb_;

    std::unique_ptr<shell::ILauncherTheme>      theme_;
    std::vector<shell::LauncherEntry>           entries_;
    std::vector<std::unique_ptr<nema::anim::AnimationPlayer>> players_;  // T2 icon players
    char title_[24] = "PALANU";
    int  cursor_    = 0;

    void buildEntries();
    void activate(int i);
};

} // namespace nema
