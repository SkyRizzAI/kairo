#pragma once
#include "kairo/ui/component_screen.h"
#include <cstdint>

namespace kairo {

class Runtime;

// Display settings — component-migrated (Plan 30). Sleep / Lock / UI-Scale as
// Select controls + an FPS-overlay Toggle. Changes apply immediately + persist.
class SleepSettingsScreen : public ComponentScreen {
public:
    explicit SleepSettingsScreen(Runtime& rt);
    void        enter() override;
    ui::UiNode* build(ui::NodeArena& a, Runtime& rt) override;

private:
    struct Option { const char* label; uint64_t ms; };
    static const Option kSleepOpts[];
    static const Option kLockOpts[];
    static constexpr int kSleepCount = 5;
    static constexpr int kLockCount  = 5;
    static const char*  kScaleLabels[];
    static const float  kScaleValues[];
    static constexpr int kScaleCount = 8;

    int  findSleepIdx() const;
    int  findLockIdx()  const;
    int  findScaleIdx() const;
    void applyScale(int idx);

    void cycleSleep(int dir);
    void cycleLock(int dir);
    void cycleScale(int dir);
    void toggleFps();

    static void sleepAdj(void* u, int dir);
    static void lockAdj(void* u, int dir);
    static void scaleAdj(void* u, int dir);
    static void onFps(void* u);

    ui::ScrollState scroll_;
    int sleepIdx_ = 0, lockIdx_ = 0, scaleIdx_ = 0;
};

} // namespace kairo
