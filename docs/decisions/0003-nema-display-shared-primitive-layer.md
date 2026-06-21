# 3. `nema::display` shared primitive layer (fonts + Canvas stay core)

- Status: accepted
- Date: 2026-06-21
- Refines: [ADR 0002](0002-nema-aether-boundary.md) (nema ↔ aether boundary)

## Context

ADR 0002 set the rule: core (`nema`) keeps the raw display base, all presentation
moves to the swappable `aether` server, dependency is one-way `aether → nema`.

Executing Plan 80 hit a wall when applying that rule literally to **text/fonts**.
Plan 80 originally said: strip `drawText`/`setFont`/`BitmapFont` out of `Canvas`
into an aether `TextRenderer`. But:

- `Canvas` lives in core and its default font + text helpers are used pervasively.
- If `BitmapFont`/`FONT_*` move to `aether`, then `Canvas` (core) referencing them
  creates a **core → aether** dependency — the exact thing the boundary forbids —
  forcing a full Canvas-text extraction (46 call sites) just to compile.
- **Both** display servers need text: Aether draws widgets, and FbCon is a *text*
  console. Fonts are not Aether-specific; they are a primitive every server shares.

A second case: `IDisplayServer::renderFrame(..., const StatusBarData&)` and the
status-bar layout constants (`SEP1_Y`, `CHAR_W/H`, …) are referenced by core
(`AppHost`) *and* every server.

## Decision

Introduce a **shared display-primitive layer** in core under namespace
`nema::display`, sitting between the pure kernel and the swappable servers. It
holds only the truly low-level, server-agnostic types every display server builds
on:

- `BitmapFont` + glyph-metric helpers + the `FONT_*` tables + `FontRegistry` + the
  font handles.
- The shared layout/text-metric constants (`ui_constants.h`: `STATUS_Y`, `SEP1_Y`,
  `CONTENT_Y`, `CHAR_W/H`, `footerY()` …).

`Canvas` therefore **keeps its text API**, drawing glyphs via
`nema::display::BitmapFont` — no extraction, no core→server dependency. `IScreen`
and `ViewDispatcher` likewise stay in core as the shared view base (Runtime owns
the dispatcher). `StyleTokens`/themes, the widget toolkit, the renderer, the
component system, and all screens remain firmly in `aether::` (presentation).

The bar for `nema::display` membership is high: **only primitives a *second*,
unrelated server (FbCon, a future LVGL/text server) would also need.** Anything
that encodes look-and-feel (themes, widgets, role→font mapping) stays in `aether`.

## Consequences

- The `aether` lib split has **no font-induced circular dependency**; core compiles
  and links with zero `aether` references (proven by the ESP-IDF strict link).
- Canvas-text extraction (a large, risky refactor) was **avoided entirely**.
- FbCon and Aether share one font/registry implementation instead of duplicating it.
- Cost: `nema::display` is a third conceptual layer to keep disciplined. The "second
  server would need it" test is the guard against it becoming a presentation dumping
  ground. `FontRegistry` is borderline (it is infra, not a pure primitive) but is
  admitted because `Canvas::setFont(handle)` needs it and both servers resolve fonts
  through it.
- `fbcon` currently still resides in `nema_core` (it is core-dependency-only); making
  it its own `firmware/servers/fbcon/` lib is a future refinement, not a boundary fix.
