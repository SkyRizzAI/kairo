#pragma once
#include "nema/ui/component_screen.h"
#include "nema/screens/desktop_setting_screen.h"

namespace nema {

class Runtime;

// Appearances settings: theme, desktop style, launcher skin, asset pack,
// status bar, and UI scale.
class AppearancesSettingsScreen : public ComponentScreen {
public:
    explicit AppearancesSettingsScreen(Runtime& rt);
    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    static const char*  kThemeNames[];          // colour themes: default (mono) | flipper
    static constexpr int kThemeCount = 2;
    static const char*  kDesktopNames[];
    static const char*  kDesktopLabels[];
    static constexpr int kDesktopCount = 1;
    static const char*  kLauncherNames[];
    static const char*  kLauncherLabels[];
    static constexpr int kLauncherCount = 4;
    static const char*  kAssetNames[];
    static constexpr int kAssetCount = 1;

    static constexpr int kMaxFontPacks = 8;

    int  findThemeIdx() const;
    int  findNameIdx(const char* ns, const char* key, const char* const* names,
                     int count, const char* def) const;
    void applyTheme(int idx);
    void cycleTheme(int dir);
    void toggleDark();
    void cycleDesktop(int dir);
    void cycleLauncher(int dir);
    void cycleAsset(int dir);
    void cycleFont(int dir);
    void applyFont(const char* name);
    void openDesktopSetting();
    void scanFontPacks();

    static void themeAdj       (void* u, int dir);
    static void darkAdj        (void* u, int dir);
    static void desktopAdj     (void* u, int dir);
    static void launcherAdj    (void* u, int dir);
    static void assetAdj       (void* u, int dir);
    static void fontAdj        (void* u, int dir);
    static void onDesktopSetting(void* u);

    DesktopSettingScreen    desktopSetting_;
    aether::ui::ScrollState scroll_;
    int themeIdx_ = 0;
    int desktopIdx_ = 0, launcherIdx_ = 0, assetIdx_ = 0;
    bool darkOn_   = false;   // Plan 92 Fase B — dark mode

    // Font pack cycling
    char fontName_[48]                       = "builtin";
    char fontPackNames_[kMaxFontPacks][48]   = {};
    char fontPackPaths_[kMaxFontPacks][96]   = {};
    int  fontPackCount_                      = 0;
    int  fontPackIdx_                        = 0;
};

} // namespace nema
