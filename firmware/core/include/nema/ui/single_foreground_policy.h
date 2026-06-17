#pragma once
// Plan 55 — SingleForegroundPolicy: concrete IWindowPolicy wrapping AppHostManager.
//
// One foreground surface at a time. When a new surface is created while another
// is in the foreground, the existing one is paused (AppHostManager handles the
// pause/resume lifecycle). visibleSurfaces() returns {foreground} only.
// This matches the existing AppHostManager single-slot behaviour and is the v1
// policy that ships with Palanu.
#include "nema/ui/window_policy.h"

namespace nema {

class AppHostManager;

class SingleForegroundPolicy final : public IWindowPolicy {
public:
    explicit SingleForegroundPolicy(AppHostManager& mgr) : mgr_(mgr) {}

    void onSurfaceCreated(ISurface& s) override;
    void onSurfaceDestroyed(ISurface& s) override;
    void visibleSurfaces(std::vector<ISurface*>& out) override;
    ISurface* focused() override;

private:
    AppHostManager& mgr_;
    ISurface*       foreground_ = nullptr;
};

} // namespace nema
