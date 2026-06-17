#pragma once
#include "nema/app/component_app.h"
#include <cstdint>

namespace nema {

// TouchTestApp — fullscreen touch diagnostic (Plan 29 bring-up).
//
// Draws a crosshair at the last touch point + a 3×3 zone grid (the touched zone
// highlights) + live x/y/phase/tap-count text. Lets you verify, at a glance:
//  • touch reaches the app (crosshair appears)
//  • coordinates are correct (crosshair sits under your finger)
//  • orientation is right (tap top-left → crosshair top-left, zone TL lights)
// Uses drawRaw() + onPointer() (no Pressable) so it's a pure pointer probe.
// Exit with Back (hold center on SkyRizz).
class TouchTestApp : public ComponentApp {
public:
    const char* id()   const override { return "com.palanu.touchtest"; }
    const char* name() const override { return "Touch Test"; }
    bool fullscreen()  const override { return true; }

protected:
    ui::UiNode* build(ui::NodeArena& a, AppContext& ctx) override;  // unused (drawRaw)
    bool drawRaw(Canvas& c, AppContext& ctx) override;
    bool onPointer(const input::PointerEvent& e, AppContext& ctx) override;

private:
    int      x_     = -1;     // last touch (logical px); -1 = none yet
    int      y_     = -1;
    bool     down_  = false;
    uint32_t taps_  = 0;
};

} // namespace nema
