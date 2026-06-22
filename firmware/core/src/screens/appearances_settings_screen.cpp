// Appearances settings: theme, desktop, launcher, asset pack.
#include "nema/screens/appearances_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/aether_server.h"
#include "nema/config/config_store.h"
#include <cstring>

namespace nema {

using namespace aether::ui;

const char* AppearancesSettingsScreen::kThemeNames[kThemeCount] =
    {"flipper", "compact", "large"};
const char* AppearancesSettingsScreen::kDesktopNames[kDesktopCount]   = {"livewal"};
const char* AppearancesSettingsScreen::kDesktopLabels[kDesktopCount]  = {"live wallpaper"};
const char* AppearancesSettingsScreen::kLauncherNames[kLauncherCount] = {"playsta", "wii", "compact", "flipper"};
const char* AppearancesSettingsScreen::kLauncherLabels[kLauncherCount]= {"Playstation 5", "Nintendo WII", "Compact", "Flipper"};
const char* AppearancesSettingsScreen::kAssetNames[kAssetCount]       = {"palanu"};

AppearancesSettingsScreen::AppearancesSettingsScreen(Runtime& rt)
    : ComponentScreen(rt), desktopSetting_(rt) {}

int AppearancesSettingsScreen::findThemeIdx() const {
    std::string cur = rt_.config().getString("aether", "theme", kThemeNames[0]);
    for (int i = 0; i < kThemeCount; i++)
        if (cur == kThemeNames[i]) return i;
    return 0;
}
int AppearancesSettingsScreen::findNameIdx(const char* ns, const char* key,
                                            const char* const* names, int count,
                                            const char* def) const {
    std::string cur = rt_.config().getString(ns, key, def);
    for (int i = 0; i < count; i++)
        if (cur == names[i]) return i;
    return 0;
}

void AppearancesSettingsScreen::applyTheme(int idx) {
    const char* name = kThemeNames[idx];
    const aether::StyleTokens* t;
    if      (std::strcmp(name, "compact") == 0) t = &aether::compactTheme();
    else if (std::strcmp(name, "large")   == 0) t = &aether::largeTheme();
    else                                        t = &aether::defaultTheme();
    if (auto* srv = rt_.displayServer();
        srv && std::strcmp(srv->name(), "aether") == 0)
        static_cast<AetherServer*>(srv)->setTheme(*t);
    rt_.config().setString("aether", "theme", name);
    rt_.view().requestRedraw();
}
void AppearancesSettingsScreen::cycleTheme(int dir) {
    themeIdx_ = (themeIdx_ + dir + kThemeCount) % kThemeCount;
    applyTheme(themeIdx_);
}
void AppearancesSettingsScreen::cycleDesktop(int dir) {
    desktopIdx_ = (desktopIdx_ + dir + kDesktopCount) % kDesktopCount;
    rt_.config().setString("aether", "desktop", kDesktopNames[desktopIdx_]);
}
void AppearancesSettingsScreen::cycleLauncher(int dir) {
    launcherIdx_ = (launcherIdx_ + dir + kLauncherCount) % kLauncherCount;
    rt_.config().setString("aether", "launcher", kLauncherNames[launcherIdx_]);
}
void AppearancesSettingsScreen::cycleAsset(int dir) {
    assetIdx_ = (assetIdx_ + dir + kAssetCount) % kAssetCount;
    rt_.config().setString("aether", "assets", kAssetNames[assetIdx_]);
}
void AppearancesSettingsScreen::openDesktopSetting() {
    rt_.view().navigate(desktopSetting_);
}

void AppearancesSettingsScreen::themeAdj(void* u, int d)    { static_cast<AppearancesSettingsScreen*>(u)->cycleTheme(d); }
void AppearancesSettingsScreen::desktopAdj(void* u, int d)  { static_cast<AppearancesSettingsScreen*>(u)->cycleDesktop(d); }
void AppearancesSettingsScreen::launcherAdj(void* u, int d) { static_cast<AppearancesSettingsScreen*>(u)->cycleLauncher(d); }
void AppearancesSettingsScreen::assetAdj(void* u, int d)    { static_cast<AppearancesSettingsScreen*>(u)->cycleAsset(d); }
void AppearancesSettingsScreen::onDesktopSetting(void* u)   { static_cast<AppearancesSettingsScreen*>(u)->openDesktopSetting(); }

void AppearancesSettingsScreen::onResume() {
    themeIdx_    = findThemeIdx();
    desktopIdx_  = findNameIdx("aether", "desktop",  kDesktopNames,  kDesktopCount,  kDesktopNames[0]);
    launcherIdx_ = findNameIdx("aether", "launcher", kLauncherNames, kLauncherCount, kLauncherNames[0]);
    assetIdx_    = findNameIdx("aether", "assets",   kAssetNames,    kAssetCount,    kAssetNames[0]);
    scroll_.scrollMain   = 0;
    state_.focus.focused = 0;
    rt_.view().requestRedraw();
}

UiNode* AppearancesSettingsScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    auto input = [&](const char* label, const char* value, void (*adj)(void*, int)) {
        ListInput e; e.label = label; e.value = value; e.onAdjust = adj; e.user = this;
        return ListInputRow(a, e);
    };
    auto nav = [&](const char* label, void (*press)(void*)) {
        ListEntry e; e.label = label; e.chevron = true; e.onPress = press; e.user = this;
        return ListItemRow(a, e);
    };

    return View(a, root, {
        ListContainer(a, scroll_, {
            input("Theme",          kThemeNames[themeIdx_],        themeAdj),
            input("Desktop",        kDesktopLabels[desktopIdx_],   desktopAdj),
            nav  ("Desktop Setting",                               onDesktopSetting),
            input("Launcher",       kLauncherLabels[launcherIdx_], launcherAdj),
            input("Asset Pack",     kAssetNames[assetIdx_],        assetAdj),
        }),
    });
}

} // namespace nema
