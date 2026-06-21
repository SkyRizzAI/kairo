#pragma once
// Plan 81 — Nintendo Wii launcher skin ("wii").
// Channel-style 2-column tile grid: each entry is a rounded "channel" tile with a
// centered icon and a label beneath; the focused tile gets a bold inner border. A
// vertical scrollbar appears when the grid overflows the content area.
#include "nema/shell/launcher_theme.h"

namespace nema::shell {

class WiiLauncher : public ILauncherTheme {
public:
    const char* name()    const override { return "wii"; }
    int         columns() const override { return 2; }   // 2-column grid
    void        draw(nema::Canvas& c, const LauncherModel& m, int cursor) override;
};

} // namespace nema::shell
