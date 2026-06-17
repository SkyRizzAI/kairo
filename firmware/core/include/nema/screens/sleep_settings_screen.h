#pragma once
#include "nema/ui/component_screen.h"
#include <cstdint>

namespace nema {

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
    static const char*  kThemeNames[];      // "default" | "compact" | "large"
    static constexpr int kThemeCount = 3;

    int  findSleepIdx() const;
    int  findLockIdx()  const;
    int  findThemeIdx() const;
    void applyTheme(int idx);

    void cycleSleep(int dir);
    void cycleLock(int dir);
    void cycleTheme(int dir);
    void toggleFps();

    static void sleepAdj(void* u, int dir);
    static void lockAdj(void* u, int dir);
    static void themeAdj(void* u, int dir);
    static void onFps(void* u);

    ui::ScrollState scroll_;
    int sleepIdx_ = 0, lockIdx_ = 0, themeIdx_ = 0;

    // Display info strings — formatted in enter(), used by build().
    char infoLogical_[16]  = {};  // "264x176"
    char infoPhysical_[16] = {};  // "528x352"
    char infoScale_[16]    = {};  // "2x" / "1.5x"
};

} // namespace nema
