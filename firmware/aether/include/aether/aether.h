#pragma once
// Aether display server (Plan 80 / ADR 0002) — umbrella header.
//
// Aether is the 1-bit canvas UI display server. Everything presentation lives
// under namespace `aether::`: fonts & text, themes (StyleTokens), the UiNode/
// layout/widget model, the renderer, the component system, view stack, and the
// system screens. It depends one-way on nema core (Canvas, IDisplayDriver,
// IDisplayServer) — core never includes anything from here.
//
// Presentation sources migrate into this module phase-by-phase per Plan 80.

namespace aether {

// Build/identity marker for the module (placeholder until Phase 1 lands content).
const char* version();

}  // namespace aether
