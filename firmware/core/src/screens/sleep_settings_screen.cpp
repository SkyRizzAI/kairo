// Display settings (Plan 30 / Plan 60): sleep/lock timeouts, FPS overlay, info.
#include "nema/screens/sleep_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/canvas.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/display_server.h"
#include "nema/services/display_power_manager.h"
#include "nema/config/config_store.h"
#include "nema/service/service_container.h"
#include "nema/hal/display.h"
#include "nema/input/i_touch_driver.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

const float SleepSettingsScreen::kScaleVals[kScaleCount] =
    {1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
const char* SleepSettingsScreen::kScaleLabels[kScaleCount] =
    {"1x", "1.25x", "1.5x", "1.75x", "2x"};
// Rotation labels — plain ASCII (the degree glyph isn't in the bitmap fonts).
const char* SleepSettingsScreen::kRotLabels[kRotCount] =
    {"0", "90", "180", "270"};

const SleepSettingsScreen::Option SleepSettingsScreen::kSleepOpts[kSleepCount] = {
    {"15s", 15000}, {"30s", 30000}, {"1min", 60000}, {"5min", 300000},
    {"Off", DisplayPowerManager::kNever},
};
const SleepSettingsScreen::Option SleepSettingsScreen::kLockOpts[kLockCount] = {
    {"15s", 15000}, {"30s", 30000}, {"1min", 60000}, {"5min", 300000},
    {"Off", DisplayPowerManager::kNever},
};

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
int SleepSettingsScreen::findScaleIdx() const {
    float cur = rt_.canvas().scale();
    int best = 0; float bestErr = 1e9f;
    for (int i = 0; i < kScaleCount; i++) {
        float err = cur > kScaleVals[i] ? cur - kScaleVals[i] : kScaleVals[i] - cur;
        if (err < bestErr) { bestErr = err; best = i; }
    }
    return best;
}
void SleepSettingsScreen::cycleScale(int dir) {
    scaleIdx_ = (scaleIdx_ + dir + kScaleCount) % kScaleCount;
    float s = kScaleVals[scaleIdx_];
    if (auto* srv = rt_.displayServer()) srv->setServerScale(s);
    rt_.canvas().setScale(s);
    rt_.config().setInt("aether", "scale", (int64_t)(s * 100.0f + 0.5f));
    rt_.view().requestRedraw();
}
int SleepSettingsScreen::findRotIdx() const {
    return (int)(rt_.config().getIntOr("display", "rotation", 0) & 3);
}
void SleepSettingsScreen::cycleRotation(int dir) {
    rotIdx_ = (rotIdx_ + dir + kRotCount) % kRotCount;
    rt_.config().setInt("display", "rotation", (int64_t)rotIdx_);
    // Apply live where the driver supports it (simulator + hardware): the display
    // swaps its logical dims and the UI reflows; touch follows. On a driver with
    // no live support these are no-ops and the persisted value applies at boot.
    if (auto* disp  = rt_.container().resolve<IDisplayDriver>()) disp->setRotation((uint8_t)rotIdx_);
    if (auto* touch = rt_.container().resolve<ITouchDriver>())   touch->setRotation((uint8_t)rotIdx_);
    // Announce so each board's IKeyMap can remap its directional buttons (Plan 92).
    char rbuf[4]; std::snprintf(rbuf, sizeof(rbuf), "%d", rotIdx_);
    rt_.events().publish(Event{events::DisplayRotationChanged, {{"rotation", rbuf}}});
    rt_.view().requestRedraw();
}
void SleepSettingsScreen::toggleFps() {
    bool on = !rt_.showFps();
    rt_.setShowFps(on);
    rt_.config().setInt("aether", "fps", on ? 1 : 0);
}
void SleepSettingsScreen::toggleStatusBar() {
    bool on = rt_.config().getIntOr("aether", "statusbar", 1) == 0;
    rt_.config().setInt("aether", "statusbar", on ? 1 : 0);
    rt_.view().requestRedraw();
}


void SleepSettingsScreen::onResume() {
    sleepIdx_ = findSleepIdx();
    lockIdx_  = findLockIdx();
    scaleIdx_ = findScaleIdx();
    rotIdx_   = findRotIdx();

    auto& c = rt_.canvas();
    auto lw = c.width(), lh = c.height();
    auto pw = (uint16_t)(lw * c.scale()), ph = (uint16_t)(lh * c.scale());
    std::snprintf(infoLogical_,  sizeof(infoLogical_),  "%ux%u", lw, lh);
    std::snprintf(infoPhysical_, sizeof(infoPhysical_), "%ux%u", pw, ph);
    std::snprintf(infoScale_,    sizeof(infoScale_),    "%.4gx", (double)c.scale());

    rt_.view().requestRedraw();
}

#define S(u) static_cast<SleepSettingsScreen*>(u)

UiNode* SleepSettingsScreen::build(NodeArena& a, Runtime&) {
    MenuBuilder m(a, scroll_, this);
    m.input ("Sleep After",       kSleepOpts[sleepIdx_].label, [](void* u, int d){ S(u)->cycleSleep(d); });
    m.input ("Lock Screen After", kLockOpts[lockIdx_].label,   [](void* u, int d){ S(u)->cycleLock(d); });
    m.toggle("Debug FPS",         rt_.showFps(),                       [](void* u){ S(u)->toggleFps(); });
    m.toggle("Status Bar",        rt_.config().getIntOr("aether", "statusbar", 1) != 0, [](void* u){ S(u)->toggleStatusBar(); });
    m.input ("UI Scale",          kScaleLabels[scaleIdx_],     [](void* u, int d){ S(u)->cycleScale(d); });
    m.input ("Rotation",          kRotLabels[rotIdx_],         [](void* u, int d){ S(u)->cycleRotation(d); });
    m.section("Info");
    m.info("Logical",  infoLogical_);
    m.info("Physical", infoPhysical_);
    m.info("Scale",    infoScale_);
    return m.build();
}

#undef S

} // namespace nema
