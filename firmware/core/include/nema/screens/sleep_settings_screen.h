#pragma once
#include "nema/ui/component_screen.h"
#include <cstdint>

namespace nema {

class Runtime;

// Display settings (Plan 30 / Plan 60): Sleep / Lock timeouts, FPS overlay toggle,
// and read-only display info (logical resolution, physical resolution, scale).
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
    static const float  kScaleVals[];
    static const char*  kScaleLabels[];
    static constexpr int kScaleCount = 5;
    static const char*  kRotLabels[];
    static constexpr int kRotCount = 4;   // 0°/90°/180°/270° (Plan 92 Fase A)

    int  findSleepIdx() const;
    int  findLockIdx()  const;
    int  findScaleIdx() const;
    int  findRotIdx()   const;
    void cycleSleep(int dir);
    void cycleLock(int dir);
    void cycleScale(int dir);
    void cycleRotation(int dir);
    void toggleFps();
    void toggleStatusBar();

    static void sleepAdj (void* u, int dir);
    static void lockAdj  (void* u, int dir);
    static void scaleAdj (void* u, int dir);
    static void rotAdj   (void* u, int dir);
    static void fpsAdj   (void* u, int dir);
    static void statusAdj(void* u, int dir);

    aether::ui::ScrollState scroll_;
    int sleepIdx_ = 0, lockIdx_ = 0, scaleIdx_ = 0, rotIdx_ = 0;

    char infoLogical_ [16] = {};
    char infoPhysical_[16] = {};
    char infoScale_   [16] = {};
};

} // namespace nema
