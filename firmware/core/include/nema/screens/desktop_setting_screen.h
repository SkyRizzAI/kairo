#pragma once
// Plan 81 — DesktopSettingScreen: the wallpaper sub-screen reached from
// Display & Appearances → "Desktop Setting ›". Edits how the live wallpaper is
// placed: which wallpaper, Fit mode, and Anchor. Each change persists under the
// "desktop" config namespace; the Desktop re-reads it on resume.
#include "nema/ui/component_screen.h"

namespace nema {

class Runtime;

class DesktopSettingScreen : public ComponentScreen {
public:
    explicit DesktopSettingScreen(Runtime& rt);
    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    static const char*  kWallNames[];   // selectable wallpapers (one for now)
    static constexpr int kWallCount = 1;

    int  findFitIdx()    const;
    int  findAnchorIdx() const;
    int  findWallIdx()   const;
    void cycleFit(int dir);
    void cycleAnchor(int dir);
    void cycleWall(int dir);

    static void fitAdj(void* u, int dir);
    static void anchorAdj(void* u, int dir);
    static void wallAdj(void* u, int dir);

    aether::ui::ScrollState scroll_;
    int fitIdx_ = 0, anchorIdx_ = 0, wallIdx_ = 0;
};

} // namespace nema
