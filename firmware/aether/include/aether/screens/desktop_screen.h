#pragma once
// Plan 81 — DesktopScreen: the idle/boot screen showing a live wallpaper.
//
// Boot lands here. Activate (OK) opens the Launcher; Back does nothing (this is
// home). The wallpaper skin is swappable via IDesktopTheme (one skin for now:
// livewal). When the status bar is enabled the screen runs in Normal mode (the
// GUI draws the bar; the wallpaper fills the content area); when disabled it goes
// fullscreen edge-to-edge.
#include "nema/ui/component_screen.h"
#include "aether/screens/launcher_screen.h"
#include "aether/shell/desktop_theme.h"
#include "nema/screens/mission_control_screen.h"
#include <memory>

namespace nema {

class Runtime;

class DesktopScreen : public ComponentScreen {
public:
    explicit DesktopScreen(Runtime& rt);

    void        onResume() override;
    void        onPause()  override;
    void        draw(Canvas& c) override;
    void        onAction(input::Action a) override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

protected:
    bool fullscreen() const override { return !statusBar_; }

private:
    LauncherScreen                        launcher_;
    MissionControlScreen                  missionControl_;   // Up/Left from the desktop
    std::unique_ptr<shell::IDesktopTheme> theme_;
    bool                                  statusBar_ = true;
};

} // namespace nema
