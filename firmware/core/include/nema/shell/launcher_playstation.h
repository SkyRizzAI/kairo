#pragma once
// Plan 81 — PlayStation launcher skin ("playsta").
// DSi/XMB-style horizontal carousel: banner title + a large center tile with
// preview tiles either side + a position bar. Ported from the original
// HomeScreen carousel (Plan 60).
#include "nema/shell/launcher_theme.h"

namespace nema::shell {

class PlayStationLauncher : public ILauncherTheme {
public:
    const char* name()    const override { return "playsta"; }
    int         columns() const override { return 1; }   // horizontal carousel
    void        draw(nema::Canvas& c, const LauncherModel& m, int cursor) override;
};

} // namespace nema::shell
