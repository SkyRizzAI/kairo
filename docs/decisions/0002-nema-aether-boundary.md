# 0002 — Display server is an independent module; nema core owns only the raw display base

- **Status:** Accepted
- **Date:** 2026-06-20
- **Area:** core/display, ui/aether
- **Supersedes:** the namespacing decision in [Plan 60 §3](../plans/60-aether-ui-rewrite.md) that kept `text_style`, `style_tokens`, `node`, `layout` as tier-0 in `nema`.

## Context

Nema is the hardware-agnostic **core kernel**. A stated goal (Plan 43, `IDisplayServer`)
is that the display server is **swappable**: Aether (1-bit canvas UI), fbcon (text
console), and a future LVGL (colour) backend should be interchangeable by linking a
different server — **without touching core or app/screen view code**.

The current layering does not honour that goal. Presentation code is scattered across
`nema::` and `nema::ui::`, while only the tier-1 drawing toolkit reached `aether::`:

| Concern | Today | Should be |
|---|---|---|
| `IDisplayDriver` (HAL) | `nema::` | core ✓ |
| `Canvas` (1-bit surface) **+ `drawText`/`drawChar`/`BitmapFont`** | `nema::` | core surface, but **text is presentation** |
| `node` / `Style` / `TextRole` / `layout` / `widgets` / `renderer` | `nema::ui::` | **presentation → aether** |
| `style_tokens` (themes), `FontTokens` | `nema::` | **presentation → aether** |
| `text_style` (TextRole→font, measurement) | `nema::ui::` | **presentation → aether** |
| `FontRegistry` + font family | `nema::ui::` | **presentation → aether** |
| `aether::ui::draw` toolkit | `aether::ui::draw` | aether ✓ |

Worse, the swap **contract itself leaks**: `IDisplayServer` (`nema/ui/display_server.h`)
`#include`s `style_tokens.h` and exposes `serverTheme() → const StyleTokens*`. A core
interface that is supposed to abstract *any* server is coupled to one server's theme type.

Plan 60 framed `nema::ui` as an "optional shared toolkit that Aether uses." In practice
nothing else uses it — it **is** Aether's presentation SDK, mislabelled as core. A second
server (LVGL) would bring its own object tree and never touch `nema::ui`, so keeping
widgets/themes/fonts in core is dead weight that also blocks a clean server swap.

## Decision

Split the firmware into a **core** and a **display-server module** with a hard, one-way
dependency (`aether → nema`, never the reverse):

**`nema` (core kernel) — the raw display base only:**
- `IDisplayDriver` (HAL), `DisplayManager`, `DisplayPowerManager`.
- `Canvas` reduced to a **pure pixel/rect surface**: `drawPixel`, `fillRect`, `drawRect`,
  `fillRoundRect`, `drawRoundRect`, `invertRect`, `drawLine`, `drawBitmap`, `blitRgb565`,
  clip + logical scale. **No text, no fonts.**
- A **presentation-free `IDisplayServer`** contract (no `StyleTokens`).

**`aether` (display server — `firmware/aether/`, namespace `aether::`) — all presentation:**
- **Text & fonts**: `BitmapFont`, the font family + `FontRegistry`, and a text renderer
  that blits glyphs via `Canvas` pixel ops. `TextRole`.
- **Themes**: `StyleTokens` / `FontTokens` and `theme()`.
- **UI model**: `UiNode` / `Style` / `layout` (flexbox), `widgets`, `renderer`,
  component system (`component_screen`/`component_runtime`/`focus`/`hit_test`), animations.
- **View stack**: `IScreen` + `ViewDispatcher` (navigation). Core has **no** notion of
  "screen" — a server owns its own view model (decided 2026-06-20).
- `aether::ui::draw` toolkit, status bar, and the system **screens** written against it.

`fbcon` (the text-console server) becomes its own sibling module
(`firmware/servers/fbcon/`), also depending on `nema` core.

Mechanism: `aether` becomes its own CMake static library that links `nema_core`; a target
links exactly one display-server lib. Swapping a server = linking a different lib and
calling its factory — no core edits.

Rationale for "all text → aether" (vs. keeping `drawText` in Canvas): fonts, glyph
metrics, and text layout are presentation, and a colour/LVGL server renders text its own
way. Canvas stays a dumb framebuffer that any server paints into.

## Consequences

- **Enables the swap goal**: a board links `aether` (or later `lvgl`/`fbcon`) with zero
  core changes; core carries no presentation knowledge.
- **Load-bearing invariants** (break these and the boundary rots):
  - Core (`firmware/core`, `nema::`) MUST NOT `#include` any `aether/…` header or name an
    `aether::` symbol. Enforced by review + the explicit `NEMA_CORE_SRCS` list.
  - `Canvas` is **text-free**. Anything drawing text goes through aether's text renderer.
  - `IDisplayServer` exposes only server-neutral hooks; theme/scale specifics are
    aether-internal (scale stays as a neutral float on the contract; theme does not).
  - Screens/apps that use the widget toolkit are **aether-bound**; a non-Aether server
    needs its own UI for them.
- **Cost**: a large, mostly-mechanical refactor (~60–100 files) — namespace rename
  `nema::ui::`→`aether::`, file moves to `firmware/aether/`, include-path rewrites
  (`nema/ui/*`→`aether/*`), Canvas text extraction, and `IDisplayServer` decoupling.
  Executed in build-verified phases (see Plan 80).
- **Deferred / out of scope**: actually porting the system screens to a *second* server
  (LVGL). This decision only makes that *possible*; it does not do it.
