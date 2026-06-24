#pragma once
// Plan 90 F1.3 — Aether UI debug tools.
// Dump the UiNode tree to rt.log() for layout debugging.
// These are zero-cost when not called — no overhead in release builds.
#include "nema/ui/node.h"
#include "nema/ui/widgets.h"
#include "nema/ui/component_runtime.h"

namespace nema { class Logger; }

namespace aether::ui::debug {

// Dump the full node tree to logger. Each line shows:
//   [depth indent] TYPE x,y w×h "text" (role) key=K
// Also logs overflow count if arena overflowed.
void dumpTree(const UiNode* root, nema::Logger& log, int depth = 0);

// One-line summary: node count / capacity, overflow count, focusable count.
void dumpStats(const NodeArena& arena, const ComponentState& st, nema::Logger& log);

} // namespace aether::ui::debug
