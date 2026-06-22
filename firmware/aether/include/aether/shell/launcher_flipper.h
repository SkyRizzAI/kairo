#pragma once
#include "aether/shell/launcher_theme.h"

namespace nema::shell {

// Flipper-Zero-style list launcher skin ("flipper").
// Apps scroll as a plain list: icon on the left, label right.
// The focused row gets an inverted (filled) selection box.
// Animated icons play automatically — LauncherScreen ticks all players and
// updates e.icon to the current frame before draw() is called.
class FlipperLauncher : public ILauncherTheme {
public:
    const char* name()    const override { return "flipper"; }
    int         columns() const override { return 1; }
    void        draw(nema::Canvas& c, const LauncherModel& m, int cursor) override;
};

} // namespace nema::shell
