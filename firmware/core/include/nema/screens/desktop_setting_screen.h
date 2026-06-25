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
    // Wallpaper animations are discovered at runtime from /system/assets/anims
    // (*.panim). The selection persists under config "desktop"/"anim".
    static constexpr int kMaxWalls    = 12;
    static constexpr int kWallNameLen = 32;
    char wallNames_[kMaxWalls][kWallNameLen];   // base names (no ".panim")
    int  wallCount_ = 0;

    void scanWallpapers();   // populate wallNames_/wallCount_ + sync wallIdx_
    int  findFitIdx()    const;
    int  findAnchorIdx() const;
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
