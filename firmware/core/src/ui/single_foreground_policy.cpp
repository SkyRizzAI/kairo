// Plan 55 — SingleForegroundPolicy implementation.
#include "nema/ui/single_foreground_policy.h"
#include "nema/ui/surface.h"

namespace nema {

void SingleForegroundPolicy::onSurfaceCreated(ISurface& s) {
    // The new surface becomes foreground; the old one is implicitly paused
    // by AppHostManager (it controls the pause/resume lifecycle).
    foreground_ = &s;
}

void SingleForegroundPolicy::onSurfaceDestroyed(ISurface& s) {
    if (foreground_ == &s)
        foreground_ = nullptr;
}

void SingleForegroundPolicy::visibleSurfaces(std::vector<ISurface*>& out) {
    if (foreground_)
        out.push_back(foreground_);
}

ISurface* SingleForegroundPolicy::focused() {
    return foreground_;
}

} // namespace nema
