#pragma once
#include <vector>

// Plan 55 — IWindowPolicy: swappable window manager policy.
//
// The compositor (AetherServer) holds one IWindowPolicy. When an app lifts a
// surface it calls onSurfaceCreated(); when it exits, onSurfaceDestroyed().
// Each frame the compositor calls visibleSurfaces() for the ordered z-list to
// composite (status bar is always composited first by AetherServer itself).
// focused() returns the surface that receives keyboard/button input.
//
// v1 = SingleForegroundPolicy (the existing AppHostManager / AppHost behaviour):
//   one foreground surface, new surface pauses the old one, modal "Close &
//   Open?" if a paused surface exists. visibleSurfaces() = {foreground}.
//   The current AppHost+AppHostManager is the effective implementation of this
//   policy; this interface exists so the policy can be swapped (e.g. TilingPolicy
//   for desktop/multi-window) without touching ISurface, ProcessContext, or apps.

namespace nema {

class ISurface;

class IWindowPolicy {
public:
    virtual ~IWindowPolicy() = default;

    // A new surface was created (app called createSurface or AppHost entered).
    virtual void onSurfaceCreated(ISurface& s) = 0;

    // A surface was destroyed (app exited or was killed).
    virtual void onSurfaceDestroyed(ISurface& s) = 0;

    // Ordered list of surfaces to composite this frame (front = topmost).
    // The compositor iterates front→back; status bar is composited separately.
    virtual void visibleSurfaces(std::vector<ISurface*>& out) = 0;

    // The surface that currently owns keyboard/button input focus.
    virtual ISurface* focused() = 0;
};

} // namespace nema
