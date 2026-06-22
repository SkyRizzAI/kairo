#pragma once
// Compact / Coverflow launcher skin ("compact").
// Cards arranged in a horizontal carousel: the focused card is a square in the
// centre; neighbours shrink in width (same height) as they recede, creating the
// Flipper-Zero Momentum "rolodex" illusion. App name centred below the card.
#include "aether/shell/launcher_theme.h"

namespace nema::shell {

class CompactLauncher : public ILauncherTheme {
public:
    const char* name()    const override { return "compact"; }
    int         columns() const override { return 1; }
    void        draw(nema::Canvas& c, const LauncherModel& m, int cursor) override;
};

} // namespace nema::shell
