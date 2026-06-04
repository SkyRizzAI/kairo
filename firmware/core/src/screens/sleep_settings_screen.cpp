#include "kairo/screens/sleep_settings_screen.h"
#include "kairo/runtime.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/ui/components.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/services/display_power_manager.h"
#include "kairo/services/input_service.h"
#include "kairo/input/input_action.h"
#include "kairo/config/config_store.h"

namespace kairo {

const SleepSettingsScreen::Option SleepSettingsScreen::kSleepOpts[kSleepCount] = {
    {"15s",   15000},
    {"30s",   30000},
    {"1min",  60000},
    {"5min",  300000},
    {"Off",   DisplayPowerManager::kNever},
};

const SleepSettingsScreen::Option SleepSettingsScreen::kLockOpts[kLockCount] = {
    {"15s",   15000},
    {"30s",   30000},
    {"1min",  60000},
    {"5min",  300000},
    {"Off",   DisplayPowerManager::kNever},
};

const char* SleepSettingsScreen::kScaleLabels[kScaleCount] = {
    "1x", "1.25x", "1.5x", "1.75x", "2x", "2.5x", "3x", "5x"
};
const float SleepSettingsScreen::kScaleValues[kScaleCount] = {
    1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f, 5.0f
};

SleepSettingsScreen::SleepSettingsScreen(Runtime& rt) : rt_(rt) {}

int SleepSettingsScreen::findSleepIdx() const {
    uint64_t cur = rt_.dpm().sleepMs();
    for (int i = 0; i < kSleepCount; i++)
        if (kSleepOpts[i].ms == cur) return i;
    return 0;
}

int SleepSettingsScreen::findLockIdx() const {
    uint64_t cur = rt_.dpm().lockMs();
    for (int i = 0; i < kLockCount; i++)
        if (kLockOpts[i].ms == cur) return i;
    return 0;
}

int SleepSettingsScreen::findScaleIdx() const {
    float cur = rt_.canvas().scale();
    for (int i = 0; i < kScaleCount; i++) {
        if (cur >= kScaleValues[i] - 0.01f && cur <= kScaleValues[i] + 0.01f)
            return i;
    }
    return 0;
}

void SleepSettingsScreen::applyScale(int idx) {
    float s = kScaleValues[idx];
    rt_.canvas().setScale(s);
    rt_.config().setInt("display", "scale", (int64_t)(s * 100.0f + 0.5f));
}

void SleepSettingsScreen::enter() {
    sleepIdx_ = findSleepIdx();
    lockIdx_  = findLockIdx();
    scaleIdx_ = findScaleIdx();
    cursor_   = 0;
    rt_.view().requestRedraw();
}

void SleepSettingsScreen::update(Key key) {
    int dir = 0;
    switch (key) {
    case Key::Up:
        if (cursor_ > 0) cursor_--;
        break;
    case Key::Down:
        if (cursor_ < ROWS - 1) cursor_++;
        break;
    case Key::Left:  dir = -1; break;
    case Key::Right: dir = +1; break;
    case Key::Cancel:
        rt_.view().pop();
        return;
    default:
        break;
    }

    if (dir != 0) {
        if (cursor_ == 0) {
            sleepIdx_ = (sleepIdx_ + dir + kSleepCount) % kSleepCount;
            rt_.dpm().setSleepMs(kSleepOpts[sleepIdx_].ms);
            rt_.config().setInt("dpm", "sleep_ms", (int64_t)kSleepOpts[sleepIdx_].ms);
        } else if (cursor_ == 1) {
            lockIdx_ = (lockIdx_ + dir + kLockCount) % kLockCount;
            rt_.dpm().setLockMs(kLockOpts[lockIdx_].ms);
            rt_.config().setInt("dpm", "lock_ms", (int64_t)kLockOpts[lockIdx_].ms);
        } else {
            scaleIdx_ = (scaleIdx_ + dir + kScaleCount) % kScaleCount;
            applyScale(scaleIdx_);
        }
    }
    rt_.view().requestRedraw();
}

void SleepSettingsScreen::draw(Canvas& c) {
    uint16_t y = ui::drawTitle(c, "DISPLAY");

    const char* rows[ROWS] = {"Sleep", "Lock", "Scale"};
    const char* vals[ROWS] = {kSleepOpts[sleepIdx_].label,
                              kLockOpts[lockIdx_].label,
                              kScaleLabels[scaleIdx_]};

    for (int i = 0; i < ROWS; i++) {
        bool sel = (i == cursor_);
        uint16_t row_y = y + (uint16_t)(i * (ui::CHAR_H + 4));

        char line[32];
        // "Sleep  ◄ 15s ►" — use ASCII < > since font is 5x8
        std::snprintf(line, sizeof(line), "%-6s  < %-4s >", rows[i], vals[i]);

        if (sel) {
            uint16_t hw = c.textWidth(line) + 6;
            c.invertRect(2, row_y - 1, hw, ui::CHAR_H + 1);
        }
        c.drawText(5, row_y, line, !sel);
    }

    // Footer hint — dynamic per board (e.g. "Hold ◀/Hold ▶ change  Hold ● back")
    char footer[56];
    std::snprintf(footer, sizeof(footer), "%s/%s change  %s back",
        rt_.input().hintFor(input::Action::AdjustDown),
        rt_.input().hintFor(input::Action::AdjustUp),
        rt_.input().hintFor(input::Action::Back));
    c.drawText(4, ui::footerY(c.height()), footer, true);
}

} // namespace kairo
