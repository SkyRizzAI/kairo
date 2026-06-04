#pragma once
#include "kairo/ui/screen.h"
#include <cstdint>

namespace kairo {

class Runtime;

// Display settings: Screen Sleep timeout, Screen Lock timeout, and UI Scale.
// Left/Right cycles through preset values; changes apply immediately + persist.
class SleepSettingsScreen : public IScreen {
public:
    explicit SleepSettingsScreen(Runtime& rt);

    void enter()         override;
    void update(Key key) override;
    void draw(Canvas& c) override;

private:
    static constexpr int ROWS = 3;   // Sleep, Lock, Scale

    struct Option { const char* label; uint64_t ms; };

    static const Option kSleepOpts[];
    static const Option kLockOpts[];
    static constexpr int kSleepCount = 5;
    static constexpr int kLockCount  = 5;

    static const char*  kScaleLabels[];
    static const float  kScaleValues[];
    static constexpr int kScaleCount = 8;

    int findSleepIdx() const;
    int findLockIdx()  const;
    int findScaleIdx() const;
    void applyScale(int idx);

    Runtime& rt_;
    int cursor_    = 0;   // 0 = Sleep, 1 = Lock, 2 = Scale
    int sleepIdx_  = 0;
    int lockIdx_   = 0;
    int scaleIdx_  = 0;
};

} // namespace kairo
