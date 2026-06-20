# 80 — Aether Display-Server Modularization (nema ↔ aether boundary)

> Make the display server a **swappable module**. Core (`nema`) keeps only the raw
> display base; all presentation (UI model, widgets, themes, **text & fonts**,
> renderer, screens) becomes the **Aether** server in `firmware/aether/` under
> namespace `aether::`. Swapping servers = link a different lib, no core edits.
>
> - Decision: [ADR 0002](../decisions/0002-nema-aether-boundary.md) (supersedes Plan 60 §3 namespacing).
> - Depends on: 43 (IDisplayServer), 51 (server negotiation), 53 (StyleTokens), 60 (Aether rewrite).
> - Status: ☐ planned.

---

## 1. Goal & invariants

- One-way dependency: **`aether → nema`**, never the reverse.
- Core MUST NOT `#include "aether/…"` or name any `aether::` symbol (review + explicit
  `NEMA_CORE_SRCS`).
- `Canvas` is a **pure framebuffer** (pixels/rects/clip/scale) — **no text, no fonts**.
- `IDisplayServer` is **presentation-free** (no `StyleTokens`).
- End state: a target links exactly one server lib (`aether` today; `fbcon`/`lvgl`
  later) and the boot code calls its factory. Switching = swap the lib + 1 call.

## 2. Target layout

```
firmware/
├─ core/                 # nema:: — kernel + raw display base
│  └─ include/nema/ ...   IDisplayDriver, Canvas(pixel-only), DisplayManager,
│                         DisplayPowerManager, IDisplayServer (neutral), ISurface,
│                         IScreen + ViewDispatcher (server-neutral view stack)
├─ aether/               # aether:: — the 1-bit UI display server (NEW module)
│  ├─ include/aether/     fonts+text, StyleTokens/theme, node/Style/TextRole,
│  │                      layout, widgets, renderer, draw toolkit, component system,
│  │                      animations, status bar, screens
│  ├─ src/
│  └─ CMakeLists.txt      add_library(aether) → links nema_core
└─ servers/ (future)     fbcon, lvgl as sibling server libs
```

## 3. File classification (from the current `nema/ui/*`)

**Stays in core (`nema::`) — ultra-lean, no "screen" concept:**
| File | Note |
|---|---|
| `canvas.{h,cpp}` | strip `drawText`/`drawChar`/`drawTextScaled`/`setFont`/`textWidth*` → pure pixel surface |
| `display_server.h` | drop `#include style_tokens.h` + `serverTheme()`; keep `name`/`renderFrame`/`onAction`/`serverScale`/`requiredCaps`/`uiSdk` |
| `surface.{h}`, `key.h`, `ui_constants.h` | **review per-item in Phase 0** — likely core (neutral), but evaluate each |

> **Decided (2026-06-20):** `IScreen` + `ViewDispatcher` (the whole view/navigation
> stack) move to **aether** — core has no notion of "screens". A future server brings
> its own navigation (or depends on a later-extracted shared view-runtime). `fbcon`
> becomes its **own** server module at `firmware/servers/fbcon/`.

**Moves to aether (`aether::`):**
| Group | Files |
|---|---|
| Text & fonts | `font_registry.{h,cpp}`, `font_*.cpp` (all), `BitmapFont` (from canvas.h), new `text_renderer` (the extracted glyph blit), `text_style.{h,cpp}`, `TextRole` |
| Themes | `style_tokens.{h,cpp}`, `FontTokens` |
| UI model | `node.h` (UiNode/Style), `layout.{h,cpp}`, `widgets.{h,cpp}` |
| Render | `renderer.{h,cpp}`, `draw.{h,cpp}` (already `aether::ui::draw`) |
| Component sys | `component_runtime.{h,cpp}`, `component_screen.{h,cpp}`, `focus.{h,cpp}`, `hit_test.{h,cpp}` |
| Chrome/SDK | `aether_server.{h,cpp}`, `aether_abi.{h,cpp}`, `status_bar.{h,cpp}`, `components.{h,cpp}`, `ui_sdk.h`, `app_window.{h,cpp}`, `window_policy.h`, `single_foreground_policy.{h,cpp}` |
| Animation | `animation*.{h,cpp}`, `builtin_animations.*`, `dolphin*` |
| Assets/icons | `asset_loader.{h,cpp}`, `asset_arena.cpp`, `icon_pack.{h,cpp}`, `ascii_board_renderer.{h,cpp}` |
| Input editors | `text_input.{h,cpp}`, `virtual_keyboard.{h,cpp}` |
| **View stack** | `screen.h` (IScreen), `view_dispatcher.{h,cpp}` — navigation moves to aether |
| **Screens** | all of `screens/*` (21+21 files) — they target aether widgets |

**Separate server module:** `fbcon_server.{h,cpp}` → `firmware/servers/fbcon/` (own lib
linking `nema_core`; if it needs navigation it depends on aether's view runtime or a
later shared `ui-runtime`).

> ~143 files reference `nema/ui/` or `nema::ui`. The bulk is mechanical: include-path
> rewrite (`nema/ui/X`→`aether/X`) + namespace (`nema::ui::`→`aether::`).

## 4. Phases (each ends with host+wasm+esp32 green)

- [x] **Phase 0 — Scaffold + lock boundary.** `firmware/aether/` + `CMakeLists.txt`
  (`add_library(aether)` linking `nema_core`) + `add_subdirectory(aether)`. Builds green
  (host+wasm). ⚠ Must be **committed** before any worktree-based execution — a fresh
  worktree branches from `origin/main` and won't see uncommitted scaffold/docs. Also run
  `git submodule update --init firmware/vendor/wasm3` in fresh worktrees or CMake won't configure.
- [ ] **Phase 0.5 — Decouple the kernel from presentation (NEW — discovered during exec).**
  The coupling is **bidirectional into the kernel**, not a leaf — so this MUST land first,
  staying green, with **no file moves**:
  - `Runtime` owns presentation: `runtime.h:144-146` holds `ViewDispatcher`/`Canvas`/`GuiService`;
    `runtime.cpp:76,165` constructs them; `runtime.cpp:254` `view()` returns `ViewDispatcher&`.
    → remove that ownership / hide behind a neutral seam so core doesn't name aether types.
  - `DisplayPowerManager` uses `ViewDispatcher` + `LockScreen` directly → interface seam.
  - `app_host`/`app_host_manager`/`app_registry`/`component_app` include `view_dispatcher.h`/
    `component_runtime.h`/`aether_abi.h`/`screens/*` → seam.
  - services `gui_service`/`remote_service`/`cli_service` and `js/{js_engine,js_runtime,nema_host_impl}`
    include presentation headers → seam.
  - Goal: after this phase the kernel references **zero** would-be-`aether` types, so the
    later move can't create a circular lib dep. This is a `Runtime`-API change touching every
    `rt.view()` caller + target `main.cpp` UI bootstrap — the real bulk of the work.
- [ ] **Phase 1 — Decouple the seam.** (a) Extract text from `Canvas` into an aether
  `TextRenderer` that blits glyphs via Canvas pixel ops; move `BitmapFont` + `font_*` +
  `font_registry` to aether. (b) Make `IDisplayServer` presentation-free (theme becomes
  aether-internal; GuiService stops pushing `StyleTokens` through the contract).
  Core now builds with zero text/theme.
- [ ] **Phase 2 — Themes + text style.** Move `style_tokens`, `text_style` → `aether::`.
- [ ] **Phase 3 — UI model + render.** Move `node`/`layout`/`widgets`/`renderer`/`draw`/
  `components`/`animation` → `aether::`.
- [ ] **Phase 4 — Component system + screens + server.** Move component_runtime/screen/
  focus/hit_test, `aether_server`, status_bar, and all `screens/*`; rewrite includes +
  namespaces across apps/platforms.
- [ ] **Phase 5 — Build wiring.** Targets link the `aether` lib + call its factory;
  remove moved files from `NEMA_CORE_SRCS`; verify sim + device. Document the
  "add a new server" recipe.

## 5. Risks

- **Mid-refactor breakage** — mitigated by phase gates (every phase builds all targets)
  and the explicit `NEMA_CORE_SRCS`/`AETHER_SRCS` lists (no globs).
- **Hidden core→presentation deps** — Phase 1 surfaces them (core won't compile until
  each is cut). The `IDisplayServer`/`Canvas` seam is the main one.
- **ESP-IDF component graph** — `aether` must register as an IDF component too (dual
  host/IDF build, like `nema_core`).
- **Churn vs. value** — large mechanical diff; do it in one focused stretch, commit per
  phase so any regression is bisectable.

## Tasks

- [ ] Phase 0 scaffold + boundary lock
- [ ] Phase 1 Canvas text extraction + IDisplayServer decouple
- [ ] Phase 2 themes + text_style → aether
- [ ] Phase 3 UI model + renderer → aether
- [ ] Phase 4 component system + screens + server → aether
- [ ] Phase 5 build wiring + "new server" doc
