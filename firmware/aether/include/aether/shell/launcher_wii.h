#pragma once
// Plan 81 — Nintendo Wii launcher skin ("wii").
#include "aether/shell/launcher_theme.h"

namespace nema::shell {

class WiiLauncher : public ILauncherTheme {
public:
    const char* name()    const override { return "wii"; }
    int         columns() const override { return 2; }
    void        draw(nema::Canvas& c, const LauncherModel& m, int cursor) override;
};

} // namespace nema::shell
