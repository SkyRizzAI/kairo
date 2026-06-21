#pragma once
// Plan 81 — PlayStation launcher skin ("playsta").
#include "aether/shell/launcher_theme.h"

namespace nema::shell {

class PlayStationLauncher : public ILauncherTheme {
public:
    const char* name()    const override { return "playsta"; }
    int         columns() const override { return 1; }
    void        draw(nema::Canvas& c, const LauncherModel& m, int cursor) override;
};

} // namespace nema::shell
