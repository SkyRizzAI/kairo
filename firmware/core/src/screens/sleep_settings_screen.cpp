// Plan 60 — Display settings: + Theme select (default/compact/large).
#include "nema/screens/sleep_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/canvas.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/display_server.h"
#include "nema/services/display_power_manager.h"
#include "nema/config/config_store.h"
#include <cstdio>
#include <cstring>

namespace nema {

using namespace ui;

const SleepSettingsScreen::Option SleepSettingsScreen::kSleepOpts[kSleepCount] = {
    {"15s", 15000}, {"30s", 30000}, {"1min", 60000}, {"5min", 300000},
    {"Off", DisplayPowerManager::kNever},
};
const SleepSettingsScreen::Option SleepSettingsScreen::kLockOpts[kLockCount] = {
    {"15s", 15000}, {"30s", 30000}, {"1min", 60000}, {"5min", 300000},
    {"Off", DisplayPowerManager::kNever},
};
const char* SleepSettingsScreen::kThemeNames[kThemeCount] =
    {"default", "compact", "large"};

SleepSettingsScreen::SleepSettingsScreen(Runtime& rt) : ComponentScreen(rt) {}

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
    std::string cur = rt_.config().getString("display", "theme", "default");
    for (int i = 0; i < kThemeCount; i++)
        if (cur == kThemeNames[i]) return i;
    return 0;
}
void SleepSettingsScreen::applyTheme(int idx) {
    const char* name = kThemeNames[idx];
    const StyleTokens* t;
    if      (std::strcmp(name, "compact") == 0) t = &compactTheme();
    else if (std::strcmp(name, "large")   == 0) t = &largeTheme();
    else                                        t = &defaultTheme();
    // Set theme on the active display server only — not globally — so other
    // servers (FbCon, future backends) keep their own independent themes.
    if (auto* srv = rt_.displayServer()) srv->setServerTheme(*t);
    rt_.config().setString("display", "theme", name);
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
void SleepSettingsScreen::toggleFps() {
    bool on = !rt_.showFps();
    rt_.setShowFps(on);
    rt_.config().setInt("debug", "fps", on ? 1 : 0);
}

void SleepSettingsScreen::sleepAdj(void* u, int dir){ static_cast<SleepSettingsScreen*>(u)->cycleSleep(dir); }
void SleepSettingsScreen::lockAdj(void* u, int dir) { static_cast<SleepSettingsScreen*>(u)->cycleLock(dir); }
void SleepSettingsScreen::themeAdj(void* u, int dir){ static_cast<SleepSettingsScreen*>(u)->cycleTheme(dir); }
void SleepSettingsScreen::onFps(void* u)            { static_cast<SleepSettingsScreen*>(u)->toggleFps(); }

void SleepSettingsScreen::enter() {
    sleepIdx_ = findSleepIdx();
    lockIdx_  = findLockIdx();
    themeIdx_ = findThemeIdx();

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
    uint8_t pad = nema::theme().space.sm;
    uint8_t gap = nema::theme().space.xs;
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = pad; root.gap = gap;
    root.align = Align::Stretch;
    Style sv;   sv.dir = FlexDir::Col; sv.align = Align::Stretch; sv.gap = gap;

    return View(a, root, {
        TitleBar(a, "DISPLAY"),
        ScrollView(a, scroll_, sv, {
            Select(a, "Theme", kThemeNames[themeIdx_],      themeAdj, this),
            Select(a, "Sleep", kSleepOpts[sleepIdx_].label, sleepAdj, this),
            Select(a, "Lock",  kLockOpts[lockIdx_].label,   lockAdj,  this),
            Toggle(a, "FPS overlay", rt_.showFps(), onFps, this),
            Header(a, "INFO"),
            ListItem(a, "Logical",  infoLogical_,  nullptr, nullptr),
            ListItem(a, "Physical", infoPhysical_, nullptr, nullptr),
            ListItem(a, "Scale",    infoScale_,    nullptr, nullptr),
        }),
    });
}

} // namespace nema
