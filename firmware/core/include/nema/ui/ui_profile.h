#pragma once
// Plan 90 — UiProfile: resource limits and behavioral flags derived from board
// capabilities. Runtime exposes rt.uiProfile(). Apps and screens read it to
// adapt their node count / animations / transitions to the board's tier.
//
// Universal Floor (all boards MUST support):
//   - 128 nodes minimum, 48 focusable minimum
//   - Scroll without momentum
//
// Extended boards declare caps::UiExtended / UiMomentum / etc. in
// describeHardware(); UiProfile::fromCapabilities() builds the struct from those.
#include "nema/system/capability_registry.h"
#include "nema/system/capabilities.h"
#include <cstdint>

namespace aether::ui {

struct UiProfile {
    // Arena & focus limits
    uint16_t maxNodes     = 128;  // universal floor
    uint16_t maxFocusable = 48;   // not used as hard limit (tree-walk focus), but
                                  // exposed so apps can know the board's "budget"

    // Behavioral flags — false = feature absent or disabled for this board
    bool momentumScroll    = false;  // flick scroll + velocity decay
    bool screenTransitions = false;  // slide/fade push/pop animation
    bool valueAnimations   = false;  // tween/spring Animated API

    // Build from a CapabilityRegistry (call during Runtime init or on demand).
    static UiProfile fromCapabilities(const nema::CapabilityRegistry& caps) {
        UiProfile p;
        if (caps.has(nema::caps::UiExtended))    { p.maxNodes = 512; p.maxFocusable = 128; }
        if (caps.has(nema::caps::UiMomentum))      p.momentumScroll    = true;
        if (caps.has(nema::caps::UiTransitions))   p.screenTransitions = true;
        if (caps.has(nema::caps::UiAnimations))    p.valueAnimations   = true;
        return p;
    }
};

} // namespace aether::ui
