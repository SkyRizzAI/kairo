#pragma once
#include "nema/ui/component_screen.h"
#include "nema/screens/desktop_setting_screen.h"
#include <cstdint>

namespace nema {

class Runtime;

// Display & Appearances — component-migrated (Plan 30). Sleep / Lock / UI-Scale
// as Select controls + an FPS-overlay Toggle, plus the Plan 81 shell appearance
// rows (Theme / Desktop / Launcher / Assets Pack / Status Bar). Changes apply
// immediately + persist.
class SleepSettingsScreen : public ComponentScreen {
public:
    explicit SleepSettingsScreen(Runtime& rt);
    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    struct Option { const char* label; uint64_t ms; };
    static const Option kSleepOpts[];
    static const Option kLockOpts[];
    static constexpr int kSleepCount = 5;
    static constexpr int kLockCount  = 5;
    static const char*  kThemeNames[];      // "flipper" | "compact" | "large"
    static constexpr int kThemeCount = 3;
    static const float  kScaleVals[];       // 1.0 .. 2.0 logical-pixel scale
    static const char*  kScaleLabels[];
    static constexpr int kScaleCount = 5;
    static const char*  kDesktopNames[];    // config value: "livewal"
    static const char*  kDesktopLabels[];   // display: "live wallpaper"
    static constexpr int kDesktopCount = 1;
    static const char*  kLauncherNames[];   // config value: "playsta" | "wii"
    static const char*  kLauncherLabels[];  // display: "Playstation 5" | "Nintendo WII"
    static constexpr int kLauncherCount = 2;
    static const char*  kAssetNames[];      // "palanu" (one pack for now)
    static constexpr int kAssetCount = 1;

    int  findSleepIdx() const;
    int  findLockIdx()  const;
    int  findThemeIdx() const;
    int  findScaleIdx() const;
    int  findNameIdx(const char* ns, const char* key, const char* const* names,
                     int count, const char* def) const;
    void applyTheme(int idx);

    void cycleSleep(int dir);
    void cycleLock(int dir);
    void cycleTheme(int dir);
    void cycleScale(int dir);   // change logical pixel scale (pixelation) live + persist
    void cycleDesktop(int dir);
    void cycleLauncher(int dir);
    void cycleAsset(int dir);
    void toggleFps();
    void toggleStatusBar();
    void openDesktopSetting();

    static void sleepAdj(void* u, int dir);
    static void lockAdj(void* u, int dir);
    static void themeAdj(void* u, int dir);
    static void scaleAdj(void* u, int dir);
    static void desktopAdj(void* u, int dir);
    static void launcherAdj(void* u, int dir);
    static void assetAdj(void* u, int dir);
    static void onFps(void* u);
    static void fpsAdj(void* u, int dir);   // split-input adjust → toggle (any dir)
    static void statusAdj(void* u, int dir);
    static void onDesktopSetting(void* u);

    DesktopSettingScreen desktopSetting_;

    aether::ui::ScrollState scroll_;
    int sleepIdx_ = 0, lockIdx_ = 0, themeIdx_ = 0, scaleIdx_ = 0;
    int desktopIdx_ = 0, launcherIdx_ = 0, assetIdx_ = 0;

    // Display info strings — formatted in enter(), used by build().
    char infoLogical_[16]  = {};  // "264x176"
    char infoPhysical_[16] = {};  // "528x352"
    char infoScale_[16]    = {};  // "2x" / "1.5x"
};

} // namespace nema
