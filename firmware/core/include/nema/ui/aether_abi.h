#pragma once
#include <cstdint>

// Plan 50 — Aether ABI: flat C-compatible widget-building functions.
// These are the host-side implementations of aether:ui imports.
// Apps (WASM/JS/C) call these via generated bindings; they delegate
// to the nema::ui retained-mode widget tree + NodeArena.
//
// Convention: each function takes a parent view handle (UiNode*)
// and returns a child handle. nullptr = root / error.

#ifndef __cplusplus
#error "aether_abi.h requires C++"
#endif

namespace nema::ui { class NodeArena; }

// Opaque view handle (pointer to nema::ui::UiNode).
using AetherView = void*;

// Set/get the per-frame arena (called by the surface / Plan 55).
// The ABI functions allocate UiNodes from this arena.
void aether_set_arena(nema::ui::NodeArena* a);
nema::ui::NodeArena* aether_get_arena();

// ── view interface ──────────────────────────────────────────────────

// Begin a container. direction: "row" or "col". Returns the container handle.
AetherView aether_view_begin(const char* direction);

// End the current container. Children added after this go to the parent.
void aether_view_end(void);

// ── text interface ──────────────────────────────────────────────────

// Simple body text label. Returns the text node handle.
AetherView aether_text_label(const char* content);

// Styled text. variant: "title", "subtitle", "body", "caption".
AetherView aether_text_styled(const char* content, const char* variant);

// ── interactive interface ───────────────────────────────────────────

// Pressable button. on_press is a callback handle (runtime-specific).
AetherView aether_interactive_button(const char* label, int32_t on_press);

// ── scroll interface ────────────────────────────────────────────────

// Begin a vertical scroll region. Returns the scroll container handle.
AetherView aether_scroll_begin(void);

// End the scroll region.
void aether_scroll_end(void);
