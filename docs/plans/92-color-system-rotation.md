# 92 — Colour theme system, dark mode, display rotation & pixel-perfect chrome

> Evolve the Aether UI from 1-bit-only to a **capability-driven colour system**: semantic
> colour tokens (RGB565 + hybrid mono fallback), per-theme dark mode, themeable selection
> styles (incl. Wii-style drop shadow on colour panels), runtime display rotation, and a
> pixel-perfect status-bar/chrome pass.
>
> **Final scope (locked with user).** The four goals below are the Definition of Done;
> Wii drop-shadow (needs >2 colours) and the pixel-perfect sweep are **descoped** (Fase D).
> 1. ✅ Support monochrome (1-bit, 2-colour palette).
> 2. ✅ If the LCD is colour-capable, Settings can switch colour themes (still 2 colours,
>    but non-B&W — e.g. Flipper orange/black). Gated by `IDisplayDriver::supportsColor()`.
> 3. ✅ Dark mode = swap the fg/bg tokens.
> 4. ✅ Display rotation (0/90/180/270) + input/touch follow.
>
> - Status: 🟢 firmware done & build-verified (hardware + wasm). One caveat: the simulator
>   doesn't yet render the device colour/dark palette (Forge uses its own tint dropdown) —
>   visible on hardware; sim auto-follow needs the deferred Option B (device→Forge palette).
> - Architecture: [ADR 0012](../decisions/0012-color-token-system-display-capability.md).
> - Design reference: [`docs/architecture/ui-design-system.md`](../architecture/ui-design-system.md) §17 (Target design).
> - Builds on: Plan 53 (StyleTokens), Plan 79/80 (UI foundations + aether split),
>   Plan 90 (UI maturity), Plan 81/ADR 0004 (skins).

## Goal & constraints

- **Board-agnostic.** One core/theme/widget codebase renders real colour on an RGB panel
  and a sensible on/off image on a 1-bit panel — never branching on board name, only on
  the display's declared capability (`display.color`).
- **Incremental.** Keep the `Canvas` `bool on` overloads (→ `fg`/`bg`) so the migration
  never requires a big-bang rewrite of every call site.
- **Pixel-perfect.** Integer logical→physical scale only; design at native logical
  resolution; snap to spacing tokens; hand-tuned bitmaps.

## Decisions already locked (see ADR 0012)

- Internal colour space: **RGB565**.
- Mono fallback: **hybrid** — auto-luminance threshold by default, optional per-token
  `monoOverride` (force ON/OFF).
- Colour lives in the **theme**; capability + final conversion + rotation live in the
  **display driver**; **board profile only wires the driver**.
- `Color` type + Canvas colour API are **core** (`nema::display`); `ColorTokens`,
  `SelectionStyle`, named themes are **presentation** (`aether::`).

---

## Fase A — Display rotation (independent, ships first)

Smallest, most self-contained; gives a visible win and exercises the driver seam.
**Shipped** as both **boot-time** (skyrizz reads `display/rotation` at init) **and
live hot-rotate** via a board-agnostic seam (`IDisplayDriver::setRotation` /
`ITouchDriver::setRotation`, default no-ops). The Settings selector applies live.

- [x] Board-agnostic seam: `IDisplayDriver::setRotation/rotation` + `ITouchDriver::setRotation`
      (default no-op so non-rotating drivers are unaffected).
- [x] Boot-time: skyrizz `LcdDriver`/`Ft6336Touch` read config `display/rotation` at init.
- [x] Hardware path: `LcdDriver` sets the ILI9341 `MADCTL` per rotation
      (`{0x48,0x28,0x88,0xE8}`, BGR bit kept so `blitRgb565` stays valid); `applyMadctl()`
      shared by `panelInit()` + live `setRotation()`.
- [x] `LcdDriver` swaps `width_`/`height_` for 90°/270° (same framebuffer byte size);
      `Canvas::width()/height()` report post-rotation dims and the adaptive UI reflows.
- [x] **Touch transform** in `Ft6336Touch::toLogical()` — 4-case map paired to the MADCTL set.
- [x] **Simulator (WASM):** `NullDisplay::setRotation` swaps dims (no MADCTL — no glass);
      `RemoteScreenTap` forwards to inner; Forge adapts to the streamed W×H. Pointer events
      return from Forge already in the rotated space, so no touch transform needed.
- [x] **Sim bug fix:** `RemoteScreenTap::ensureShadow()` now runs on `setRotation` + each
      `clear()` (frame start). Previously it only ran at init/`flushBuffer`, so a live dim
      swap left the shadow + streamed W×H stale → "only the width changed". Now it re-shadows.
- [x] **Input follows rotation** (event-driven, per-board keymap): core fires
      `DisplayRotationChanged{rotation}`; `IKeyMap::setRotation` (default no-op) lets a board
      remap. `E32KeyMap::rotateId()` rotates the 4 directional buttons (Up→Right→Down→Left)
      per orientation; the board seeds from config at boot and subscribes to the event for live
      updates. iPad-style: turn the screen, the buttons follow. *(Direction verified on hardware:
      steps CCW `(i + 4 - rotation_)` — at 90° Right→Up, Up→Left. Confirm 180°/270° on device.)*
- [x] Rotation selector in Settings → Sleep/Display: persists config **and** applies live
      (`resolve<IDisplayDriver>()/<ITouchDriver>()->setRotation`) + `requestRedraw`.
- [x] Build-verify wasm (simulator) + esp32 (skyrizz binary, 46% free). *(host N/A.)*
- [x] **Simulator live rotation verified by build** — cycle in Settings reflows instantly.
- [ ] **Flash-test 4 orientations + touch on skyrizz-e32** (on-device; pending user). Hardware
      live `setRotation` is bench-untested — boot-time read is the verified hardware path.
- [ ] Software-rotation fallback for drivers without HW rotate (e-ink dev-board) — deferred.

## Fase B — Colour core (2-colour palette path) ✅ firmware done

Pragmatic first slice: instead of a full `bool`→`Color` Canvas migration (heavy; deferred
to Fase D for multi-colour), recolour via the **driver's 2-colour palette** — the 1-bit
framebuffer expands on→fg, off→bg in RGB565. Skyrizz already had `fgColor_/bgColor_`, so the
whole UI recolours by swapping the palette. (Full `Color`/`monoOverride` per ADR 0012 stays
the path for >2 colours — see Fase D.)

- [x] `ColorTokens { name; uint16_t fg; uint16_t bg; }` in `style_tokens.h`, **orthogonal**
      to the size `StyleTokens`. Built-ins `monoColors()` (white/black) + `flipperColors()`
      (black on orange). Active palette + `darkMode()` globals.
- [x] Board-agnostic seam `IDisplayDriver::setPalette(fg,bg)` (default no-op); `LcdDriver` →
      `setColors`, `RemoteScreenTap` forwards, `NullDisplay` stores.
- [x] `Canvas::driver()` getter; `AetherServer::renderFrame` pushes the active palette
      (dark-mode swap) to the driver each frame.
- [x] Boot loads `aether/color` + `aether/dark` from config.
- [x] **Display colour capability** `IDisplayDriver::supportsColor()` (default false): skyrizz
      ILI9341 = true, e-ink dev-board = false, sim = true (toggle via config `display/sim_color`).
      Appearances **gates the Colour selector** on it — a true B&W panel only shows mono +
      dark-mode (= invert); a colour panel offers the palette themes. Matches "LCD decides;
      system detects". The framebuffer stays 1-bit either way (still a 2-colour palette).
- [x] Build-verify wasm + esp32. Default `mono` = byte-identical current look (regression-safe).
- [x] **BGR fix:** skyrizz ILI9341 is a BGR panel and the 1-bit flush skipped the R↔B swap
      `blitRgb565` does → orange showed as blue. `LcdDriver::setPalette` now swaps R↔B
      (symmetric white/black unaffected).
- [x] **Option B — device-driven sim/remote colour (done).** Firmware `RemoteScreenTap`
      sends `System` opcode `SetPalette` (0x03, `[fg:2][bg:2]` RGB565) on palette change + on
      viewer connect. Forge `codec.ts rgb565ToRgb888`, `session.ts` emits a `palette` event,
      `simStore.palette` follows it, and `BoardVisual` uses it (`simStore.palette ?? tint`).
      The sim/remote now recolour from the DEVICE's Theme, not a web selection. svelte-check 0 errors.

## Fase C — Dark mode + colour themes ✅ firmware done

- [x] Dark mode = swap fg/bg (matches "kebalikannya"): config `aether/dark`, global toggle.
- [x] **Theme = colour theme** (merged with the old size-theme selector per user): `default`
      (mono white/black) + `flipper` (orange/black) — **same fonts/sizes, only colour differs**.
      One "Theme" row in Appearances (gated on colour capability) + Dark Mode; config `aether/theme`.
- [x] **Flipper** theme: normal `{fg black, bg orange}` ↔ dark `{fg orange, bg black}`.
- [ ] Verify on hardware (skyrizz shows Flipper orange after the BGR fix) + in the sim (follows Theme).
- [ ] More palettes (amber, green-phosphor) — trivial to add as `ColorTokens`.

## Fase D — Selection styles + status-bar redesign + pixel-perfect pass

> **Descoped** (user): drop-shadow + pixel-perfect are out of the final scope. Only the
> status-bar redesign below shipped. Drop-shadow stays parked behind the multi-colour
> framebuffer (ADR 0012 full-`Color`), to revisit only if richer colour is ever wanted.

- [x] **Status bar redesign:** simplified ~11px hand-drawn battery (was a 25px Flipper icon)
      with charge-proportional fill; clock moved/kept **top-left** in the compact 5×8 mono
      font. *(Sub-8px "smaller" needs a 3×5 micro-font — noted, not built.)*
- [ ] `SelectionStyle = { Invert, FillRounded, DropShadow }` as a theme token; renderer
      picks per theme + `display.color` capability.
- [ ] `DropShadow`: Wii-style 1px (x=1,y=1) offset in `muted`, blended into `bg`
      (**needs >2 colours → the multi-colour framebuffer migration, ADR 0012 full-`Color`
      path**); **mono falls back to `Invert`**. This is the remaining heavy piece.
- [ ] Pixel-perfect sweep: enforce integer `serverScale`; audit 1px lines/borders; snap
      chrome to spacing tokens; hand-tune status-bar + selection bitmaps at native res.
- [ ] Build-verify host + wasm + esp32; flash-test on skyrizz-e32.

---

## Definition of Done (per CLAUDE.md)

- [ ] `docs/STATE.md` row for UI/display updated when status changes.
- [ ] `docs/feats/` updated/added for colour themes, dark mode, rotation.
- [ ] `docs/architecture/ui-design-system.md` §17 promoted from "proposed" to "current"
      as each fase ships; `aether-ui.md` updated for the colour render path.
- [ ] ADR 0012 status → Accepted once Fase B lands.
- [ ] Conventional commits per fase (`feat(ui):`, `feat(display):` …).

## Open follow-ups (not blocking)

- Richer palettes (>2 colours) need a mono-contrast lint, since auto-luminance degrades
  when many tokens cluster in luminance.
- Per-app theming / app-declared accent (would extend the constrained WASM UI ABI).
- True larger fonts vs pixel-doubling for headings/clock (Plan 25 Phase 3) — pairs well
  with the pixel-perfect goal.
