#pragma once
#include "nema/app/component_app.h"
#include <cstdint>

namespace nema {

// TouchTestApp — fullscreen touch diagnostic (Plan 29 bring-up).
//
// Draws a marker box + center dot at the last touch point, live x/y/phase/tap
// count, and corner labels (TL/TR/BL/BR) so you can verify at a glance:
//  • touch reaches the app (marker appears),
//  • coordinates are correct (marker sits under your finger),
//  • orientation is right (tap top-left → marker top-left).
// Pure pointer probe via drawRaw()+onPointer() (no Pressable). Exit with Back.
// Launched from Settings → Touch → "Touch Test" (not in the launcher).
class TouchTestApp : public ComponentApp {
public:
    const char* id()       const override { return "com.palanu.touchtest"; }
    const char* name()     const override { return "Touch Test"; }
    bool        fullscreen() const override { return true; }
    const char* category() const override { return "System"; }   // hidden from launcher

protected:
    aether::ui::UiNode* build(aether::ui::NodeArena& a, AppContext& ctx) override;  // unused (drawRaw)
    bool drawRaw(Canvas& c, AppContext& ctx) override;
    bool onPointer(const input::PointerEvent& e, AppContext& ctx) override;

private:
    int      x_    = -1;   // last touch (logical px); -1 = none yet
    int      y_    = -1;
    bool     down_ = false;
    uint32_t taps_ = 0;
};

} // namespace nema
