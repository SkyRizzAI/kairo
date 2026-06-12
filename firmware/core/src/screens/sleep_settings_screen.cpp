#include "nema/screens/sleep_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/canvas.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/services/display_power_manager.h"
#include "nema/config/config_store.h"

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
const char* SleepSettingsScreen::kScaleLabels[kScaleCount] =
    {"1x", "1.25x", "1.5x", "1.75x", "2x", "2.5x", "3x", "5x"};
const float SleepSettingsScreen::kScaleValues[kScaleCount] =
    {1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f, 5.0f};

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
int SleepSettingsScreen::findScaleIdx() const {
    float cur = rt_.canvas().scale();
    for (int i = 0; i < kScaleCount; i++)
        if (cur >= kScaleValues[i] - 0.01f && cur <= kScaleValues[i] + 0.01f) return i;
    return 0;
}
void SleepSettingsScreen::applyScale(int idx) {
    float s = kScaleValues[idx];
    rt_.canvas().setScale(s);
    rt_.config().setInt("display", "scale", (int64_t)(s * 100.0f + 0.5f));
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
void SleepSettingsScreen::cycleScale(int dir) {
    scaleIdx_ = (scaleIdx_ + dir + kScaleCount) % kScaleCount;
    applyScale(scaleIdx_);
}
void SleepSettingsScreen::toggleFps() {
    bool on = !rt_.showFps();
    rt_.setShowFps(on);
    rt_.config().setInt("debug", "fps", on ? 1 : 0);
}

void SleepSettingsScreen::sleepAdj(void* u, int dir){ static_cast<SleepSettingsScreen*>(u)->cycleSleep(dir); }
void SleepSettingsScreen::lockAdj(void* u, int dir) { static_cast<SleepSettingsScreen*>(u)->cycleLock(dir); }
void SleepSettingsScreen::scaleAdj(void* u, int dir){ static_cast<SleepSettingsScreen*>(u)->cycleScale(dir); }
void SleepSettingsScreen::onFps(void* u)            { static_cast<SleepSettingsScreen*>(u)->toggleFps(); }

void SleepSettingsScreen::enter() {
    sleepIdx_ = findSleepIdx();
    lockIdx_  = findLockIdx();
    scaleIdx_ = findScaleIdx();
    scroll_.scrollMain = 0;
    rt_.view().requestRedraw();
}

UiNode* SleepSettingsScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 3; root.gap = 1;
    Style line; line.height = 1; line.background = true;
    Style sv;   sv.dir = FlexDir::Col; sv.align = Align::Stretch; sv.gap = 3;

    return View(a, root, {
        Text(a, "DISPLAY", TextRole::Title),
        View(a, line, {}),
        ScrollView(a, scroll_, sv, {
            Select(a, "Sleep", kSleepOpts[sleepIdx_].label, sleepAdj, this),
            Select(a, "Lock",  kLockOpts[lockIdx_].label,   lockAdj,  this),
            Select(a, "Scale", kScaleLabels[scaleIdx_],     scaleAdj, this),
            Toggle(a, "FPS overlay", rt_.showFps(), onFps, this),
        }),
    });
}

} // namespace nema
