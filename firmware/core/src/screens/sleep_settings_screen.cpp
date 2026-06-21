// Plan 60 — Display settings: + Theme select (default/compact/large).
#include "nema/screens/sleep_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/canvas.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/display_server.h"
#include "nema/ui/aether_server.h"
#include "nema/services/display_power_manager.h"
#include "nema/config/config_store.h"
#include <cstdio>
#include <cstring>

namespace nema {

using namespace aether::ui;

const SleepSettingsScreen::Option SleepSettingsScreen::kSleepOpts[kSleepCount] = {
    {"15s", 15000}, {"30s", 30000}, {"1min", 60000}, {"5min", 300000},
    {"Off", DisplayPowerManager::kNever},
};
const SleepSettingsScreen::Option SleepSettingsScreen::kLockOpts[kLockCount] = {
    {"15s", 15000}, {"30s", 30000}, {"1min", 60000}, {"5min", 300000},
    {"Off", DisplayPowerManager::kNever},
};
const char* SleepSettingsScreen::kThemeNames[kThemeCount] =
    {"flipper", "compact", "large"};
const float SleepSettingsScreen::kScaleVals[kScaleCount] =
    {1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
const char* SleepSettingsScreen::kScaleLabels[kScaleCount] =
    {"1x", "1.25x", "1.5x", "1.75x", "2x"};
const char* SleepSettingsScreen::kDesktopNames[kDesktopCount]   = {"livewal"};
const char* SleepSettingsScreen::kDesktopLabels[kDesktopCount]  = {"live wallpaper"};
const char* SleepSettingsScreen::kLauncherNames[kLauncherCount] = {"playsta", "wii"};
const char* SleepSettingsScreen::kLauncherLabels[kLauncherCount]= {"Playstation 5", "Nintendo WII"};
const char* SleepSettingsScreen::kAssetNames[kAssetCount]       = {"palanu"};

SleepSettingsScreen::SleepSettingsScreen(Runtime& rt)
    : ComponentScreen(rt), desktopSetting_(rt) {}

int SleepSettingsScreen::findSleepIdx() const {
    uint64_t cur = rt_.dpm().sleepMs();
    for (int i = 0; i < kSleepCount; i++) if (kSleepOpts[i].ms == cur) return i;
    return 0;
}
int SleepSettingsScreen::findLockIdx() const {
    uint64_t cur = rt_.dpm().lockMs();
    for (int i = 0; i < kLockCount; i++) if (kLockOpts[i].ms == cur) return i;
    return 0;
}

int SleepSettingsScreen::findThemeIdx() const {
    std::string cur = rt_.config().getString("aether", "theme", kThemeNames[0]);
    for (int i = 0; i < kThemeCount; i++)
        if (cur == kThemeNames[i]) return i;
    return 0;   // unknown / legacy "default" → flipper (index 0)
}
int SleepSettingsScreen::findNameIdx(const char* ns, const char* key,
                                     const char* const* names, int count,
                                     const char* def) const {
    std::string cur = rt_.config().getString(ns, key, def);
    for (int i = 0; i < count; i++)
        if (cur == names[i]) return i;
    return 0;
}
int SleepSettingsScreen::findScaleIdx() const {
    float cur = rt_.canvas().scale();
    int best = 0; float bestErr = 1e9f;
    for (int i = 0; i < kScaleCount; i++) {
        float err = cur > kScaleVals[i] ? cur - kScaleVals[i] : kScaleVals[i] - cur;
        if (err < bestErr) { bestErr = err; best = i; }
    }
    return best;
}
void SleepSettingsScreen::applyTheme(int idx) {
    const char* name = kThemeNames[idx];
    const aether::StyleTokens* t;
    if      (std::strcmp(name, "compact") == 0) t = &aether::compactTheme();
    else if (std::strcmp(name, "large")   == 0) t = &aether::largeTheme();
    else                                        t = &aether::defaultTheme();
    // Theme is Aether-owned (ADR 0002): set it on the concrete AetherServer when
    // that's the active server. Persisted to config either way (applied at boot).
    if (auto* srv = rt_.displayServer(); srv && std::strcmp(srv->name(), "aether") == 0)
        static_cast<AetherServer*>(srv)->setTheme(*t);
    rt_.config().setString("aether", "theme", name);
    rt_.view().requestRedraw();
}

void SleepSettingsScreen::cycleSleep(int dir) {
    sleepIdx_ = (sleepIdx_ + dir + kSleepCount) % kSleepCount;
    rt_.dpm().setSleepMs(kSleepOpts[sleepIdx_].ms);
    rt_.config().setInt("dpm", "sleep_ms", (int64_t)kSleepOpts[sleepIdx_].ms);
}
void SleepSettingsScreen::cycleLock(int dir) {
    lockIdx_ = (lockIdx_ + dir + kLockCount) % kLockCount;
    rt_.dpm().setLockMs(kLockOpts[lockIdx_].ms);
    rt_.config().setInt("dpm", "lock_ms", (int64_t)kLockOpts[lockIdx_].ms);
}
void SleepSettingsScreen::cycleTheme(int dir) {
    themeIdx_ = (themeIdx_ + dir + kThemeCount) % kThemeCount;
    applyTheme(themeIdx_);
}
void SleepSettingsScreen::cycleScale(int dir) {
    scaleIdx_ = (scaleIdx_ + dir + kScaleCount) % kScaleCount;
    float s = kScaleVals[scaleIdx_];
    // Apply live: the display server owns scale (GuiService syncs canvas to it
    // each frame), so set both for an immediate effect, then persist for boot.
    if (auto* srv = rt_.displayServer()) srv->setServerScale(s);
    rt_.canvas().setScale(s);
    rt_.config().setInt("aether", "scale", (int64_t)(s * 100.0f + 0.5f));
    rt_.view().requestRedraw();
}
void SleepSettingsScreen::toggleFps() {
    bool on = !rt_.showFps();
    rt_.setShowFps(on);
    rt_.config().setInt("aether", "fps", on ? 1 : 0);
}
// Shell appearance rows (Plan 81). Each persists the selected name; the Desktop /
// Launcher screens re-read it on resume, so the new skin shows next time they open.
void SleepSettingsScreen::cycleDesktop(int dir) {
    desktopIdx_ = (desktopIdx_ + dir + kDesktopCount) % kDesktopCount;
    rt_.config().setString("aether", "desktop", kDesktopNames[desktopIdx_]);
}
void SleepSettingsScreen::cycleLauncher(int dir) {
    launcherIdx_ = (launcherIdx_ + dir + kLauncherCount) % kLauncherCount;
    rt_.config().setString("aether", "launcher", kLauncherNames[launcherIdx_]);
}
void SleepSettingsScreen::cycleAsset(int dir) {
    assetIdx_ = (assetIdx_ + dir + kAssetCount) % kAssetCount;
    rt_.config().setString("aether", "assets", kAssetNames[assetIdx_]);
}
void SleepSettingsScreen::toggleStatusBar() {
    bool on = rt_.config().getIntOr("aether", "statusbar", 1) == 0;  // flip
    rt_.config().setInt("aether", "statusbar", on ? 1 : 0);
    rt_.view().requestRedraw();
}
void SleepSettingsScreen::openDesktopSetting() {
    rt_.view().navigate(desktopSetting_);
}

void SleepSettingsScreen::sleepAdj(void* u, int dir){ static_cast<SleepSettingsScreen*>(u)->cycleSleep(dir); }
void SleepSettingsScreen::lockAdj(void* u, int dir) { static_cast<SleepSettingsScreen*>(u)->cycleLock(dir); }
void SleepSettingsScreen::themeAdj(void* u, int dir){ static_cast<SleepSettingsScreen*>(u)->cycleTheme(dir); }
void SleepSettingsScreen::scaleAdj(void* u, int dir){ static_cast<SleepSettingsScreen*>(u)->cycleScale(dir); }
void SleepSettingsScreen::desktopAdj(void* u, int dir) { static_cast<SleepSettingsScreen*>(u)->cycleDesktop(dir); }
void SleepSettingsScreen::launcherAdj(void* u, int dir){ static_cast<SleepSettingsScreen*>(u)->cycleLauncher(dir); }
void SleepSettingsScreen::assetAdj(void* u, int dir)   { static_cast<SleepSettingsScreen*>(u)->cycleAsset(dir); }
void SleepSettingsScreen::onFps(void* u)            { static_cast<SleepSettingsScreen*>(u)->toggleFps(); }
void SleepSettingsScreen::fpsAdj(void* u, int)      { static_cast<SleepSettingsScreen*>(u)->toggleFps(); }
void SleepSettingsScreen::statusAdj(void* u, int)   { static_cast<SleepSettingsScreen*>(u)->toggleStatusBar(); }
void SleepSettingsScreen::onDesktopSetting(void* u) { static_cast<SleepSettingsScreen*>(u)->openDesktopSetting(); }

void SleepSettingsScreen::onResume() {
    sleepIdx_ = findSleepIdx();
    lockIdx_  = findLockIdx();
    themeIdx_ = findThemeIdx();
    scaleIdx_ = findScaleIdx();
    desktopIdx_  = findNameIdx("aether", "desktop",  kDesktopNames,  kDesktopCount,  kDesktopNames[0]);
    launcherIdx_ = findNameIdx("aether", "launcher", kLauncherNames, kLauncherCount, kLauncherNames[0]);
    assetIdx_    = findNameIdx("aether", "assets",   kAssetNames,    kAssetCount,    kAssetNames[0]);

    // Format display info once on enter — these don't change while the screen is open.
    auto& c = rt_.canvas();
    auto lw = c.width(), lh = c.height();
    auto pw = (uint16_t)(lw * c.scale()), ph = (uint16_t)(lh * c.scale());
    std::snprintf(infoLogical_,  sizeof(infoLogical_),  "%ux%u", lw, lh);
    std::snprintf(infoPhysical_, sizeof(infoPhysical_), "%ux%u", pw, ph);
    std::snprintf(infoScale_,    sizeof(infoScale_),    "%.4gx", (double)c.scale());

    scroll_.scrollMain = 0;
    rt_.view().requestRedraw();
}

UiNode* SleepSettingsScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    // Split-input row: fixed-ratio split + always-on chevrons (the cycles wrap),
    // so values line up across every row.
    auto input = [&](const char* label, const char* value, void (*adj)(void*, int)) {
        ListInput e;
        e.label = label; e.value = value;
        e.onAdjust = adj; e.user = this;     // canPrev/canNext default true → "< value >"
        return ListInputRow(a, e);
    };
    auto info = [&](const char* label, const char* value) {
        ListEntry e; e.label = label; e.value = value;   // display-only (no onPress)
        return ListItemRow(a, e);
    };
    auto nav = [&](const char* label, void (*press)(void*)) {
        ListEntry e; e.label = label; e.chevron = true;  // navigation row "label  >"
        e.onPress = press; e.user = this;
        return ListItemRow(a, e);
    };

    return View(a, root, {
        ListContainer(a, scroll_, {
            ListSection(a, "Display"),
            input("Sleep After",       kSleepOpts[sleepIdx_].label, sleepAdj),
            input("Lock Screen After", kLockOpts[lockIdx_].label,   lockAdj),
            input("Debug FPS",         rt_.showFps() ? "ON" : "OFF", fpsAdj),
            ListSection(a, "Appearances"),
            input("Theme",          kThemeNames[themeIdx_],     themeAdj),
            input("Desktop",        kDesktopLabels[desktopIdx_],  desktopAdj),
            nav  ("Desktop Setting",                             onDesktopSetting),
            input("Launcher",       kLauncherLabels[launcherIdx_], launcherAdj),
            input("Assets Pack",    kAssetNames[assetIdx_],     assetAdj),
            input("Status Bar",     rt_.config().getIntOr("aether", "statusbar", 1) ? "ON" : "OFF",
                                                                statusAdj),
            input("UI Scale",       kScaleLabels[scaleIdx_],    scaleAdj),
            ListSection(a, "Info"),
            info("Logical",  infoLogical_),
            info("Physical", infoPhysical_),
            info("Scale",    infoScale_),
        }),
    });
}

} // namespace nema
