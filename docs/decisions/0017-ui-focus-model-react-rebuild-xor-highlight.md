# 17 — UI focus model: React-style rebuild + automatic XOR highlight; no declarative focusStyle

- **Status:** Accepted
- **Date:** 2026-06-27
- **Area:** ui/aether (focus, renderer, MenuBuilder)
- Refines: [ADR 0012](0012-color-token-system-display-capability.md) (selection style)

## Context

Aether is a retained-mode UI, but the tree is **not** retained across frames: every
frame the screen/app's `build()` runs from scratch and produces a fresh `UiNode` tree in
the arena. This is React's render model — `build()` = render, the screen's member state =
state, and **focus is just another piece of state** (`ComponentRuntime` tracks the
focused node by pointer; `MenuBuilder` tracks the focused *index*).

We explored adding a CSS-like declarative focus override — a `UiNode::focusStyle` field
that the renderer would swap in when the node was focused (the "`:focused` selector"
idea). Investigation found three problems:

1. **Dead infrastructure.** No widget or screen ever populated `focusStyle`; it was a
   field with no producers.
2. **It double-applies with the renderer's XOR highlight.** The renderer already paints
   the focused node by inverting its box — `highlightBox()` in `renderer.cpp`, which does
   a rounded inset XOR when `Style::selectBox` is set and a plain `invertRect` otherwise.
   A style swap layered on top of that invert would fight it (apply colour/border, then
   invert the result) — two mechanisms expressing the same intent, with the visual being
   the XOR of both.
3. **It is strictly weaker than what the rebuild already enables.** Because `build()`
   re-runs every frame with focus available as a flag, focus-dependent **content** —
   swap an icon, show/hide a chevron, animate a glyph, change a label — is expressed as
   ordinary conditional rendering inside `build()`. A visual-only `focusStyle` could only
   restyle the *same* node; conditional rendering can emit a *different subtree*.

## Decision

Do not add `focusStyle`. Focus has exactly two mechanisms, both already present:

1. **Automatic visual highlight (the default).** The renderer paints the focused node
   (`n == focused`, or `n->selfHighlight` for VirtualList items) via `highlightBox()`.
   `Style::selectBox = true` selects the Flipper-style rounded inset XOR box (used by
   `ListItemRow` and JS `Pressable`); otherwise a full-bleed `invertRect`. No node has to
   describe its focused appearance — being focused *is* the appearance.

2. **React-style conditional rendering for custom cases.** When focus must change
   *content* (not just invert pixels), `build()` reads the focused flag and emits the
   appropriate subtree — ordinary `focused ? a : b` conditional rendering, since the screen
   rebuilds every frame and knows the focused index (`state_.focus.focused`). No dedicated
   widget hook is provided: an earlier `MenuBuilder::item(renderRow)`/`willFocus()` pair was
   added for this and then removed as dead infra (no caller needed it — the same lesson as
   `focusStyle`). If a real consumer appears, it should be added in C++ **and** exposed to
   the JS runtime together (see [ADR 0018](0018-ui-component-runtime-parity.md)).

Focus context also propagates to descendants: the renderer threads an `inFocused` flag
down the tree (`childIn = inFocused || (n == focused) || n->selfHighlight`) so that, e.g.,
a `SmartLabel`/`TextRole::Smart` child of a focused row knows to marquee instead of
ellipsize.

## Consequences

- **One mental model.** Focus is state; the frame is a pure function of state. There is no
  second, declarative styling path to reconcile against the imperative one, and no
  double-apply bug class.
- **Embedded-cheap.** No selector engine, no per-node style-override storage, no
  match/cascade step — just a pointer compare in the renderer and a flag passed down.
- **Content variation is explicit code.** Anything richer than "invert the focused box"
  is written as a conditional in `build()`. This is more verbose than a declarative
  override would have been, but strictly more capable, and it lives where the rest of the
  view logic already is.
- **`inFocused` must keep propagating.** Marquee-on-focus (and any future focus-aware
  descendant behaviour) depends on the renderer threading the flag through every
  container; breaking that propagation silently disables marquee on nested labels.
- **Ruled out:** a declarative `UiNode::focusStyle` / `:focused` override layer. Removed
  as dead, conflicting, and redundant with conditional rendering.
