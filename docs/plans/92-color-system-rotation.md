# 92 — Colour theme system, dark mode, display rotation & pixel-perfect chrome

> Evolve the Aether UI from 1-bit-only to a **capability-driven colour system**: semantic
> colour tokens (RGB565 + hybrid mono fallback), per-theme dark mode, themeable selection
> styles (incl. Wii-style drop shadow on colour panels), runtime display rotation, and a
> pixel-perfect status-bar/chrome pass.
>
> - Status: 🟡 planned (concept locked; not started).
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

- [ ] Add rotation capability to the display driver interface: `setRotation(Rotation)`
      with `Rotation = {Deg0, Deg90, Deg180, Deg270}`; default supplied by the board.
- [ ] Hardware path: drive the panel's `MADCTL` register (ST7789/ILI9341) for HW rotation.
- [ ] Software fallback: rotate the framebuffer in `flush()` for drivers without HW rotate.
- [ ] `Canvas::width()/height()` report **post-rotation** logical dims (swap for 90/270);
      verify adaptive layout reflows with no per-screen change.
- [ ] **Transform touch/pointer coordinates** with the same rotation (the known trap).
- [ ] Persist to config `display/rotation`; default from board profile/driver.
- [ ] Optional: rotation selector in Settings → Sleep/Display (or Appearances).
- [ ] Build-verify host + wasm + esp32; flash-test all four orientations + touch on skyrizz-e32.

## Fase B — Colour core (the load-bearing migration)

- [ ] Define `Color { uint16_t rgb565; uint8_t monoOverride; }` and helpers (named RGB565
      constants, `rgb()` builder) in `nema::display`.
- [ ] Implement `bool toMono(Color)` — auto-luminance threshold + override handling.
- [ ] Generalise the driver colour capability: declare `monochrome | rgb565` (extend the
      existing `supportsRgb565()` notion) → surface as capability `display.color`.
- [ ] Add colour-typed `Canvas` primitives (`drawPixel/fillRect/drawRect/drawText/
      drawBitmap(...)`-with-`Color`); on mono displays route through `toMono()`.
- [ ] Keep legacy `bool on` overloads mapping to `fg`/`bg` of the active theme.
- [ ] Add `ColorTokens` to the theme struct; give the **default monochrome theme**
      (white-on-black) so existing look is byte-identical.
- [ ] Migrate the renderer + `draw.h` + core widgets to resolve colour via tokens (rows =
      `fg`/`bg`; selection = `accent`/`accentFg`).
- [ ] Build-verify host + wasm + esp32; flash-test skyrizz-e32 looks unchanged (regression gate).

## Fase C — Dark mode + colour themes

- [ ] Theme holds two `ColorTokens` sets: `light` + `dark`; global toggle via config
      `display/dark_mode`; resolve picks the active set.
- [ ] Author the **Flipper** theme: light `{bg: orange, fg: black, ...}`, dark `{bg: black,
      fg: orange, ...}`; set `monoOverride` where auto-luminance is ambiguous.
- [ ] Theme + dark-mode selectors in Settings → Appearances (extend the existing theme cycle).
- [ ] Verify Flipper theme on a colour path (simulator/host RGB) **and** its mono fallback
      on skyrizz-e32.

## Fase D — Selection styles + status-bar redesign + pixel-perfect pass

- [ ] `SelectionStyle = { Invert, FillRounded, DropShadow }` as a theme token; renderer
      picks per theme + `display.color` capability.
- [ ] `DropShadow`: Wii-style 1px (x=1,y=1) offset in `muted`, blended into `bg`
      (colour-only); **mono falls back to `Invert`**.
- [ ] Status bar redesign: simpler battery glyph; clock moved **top-left**, rendered in
      the `Tiny` font handle (smaller); re-tune indicator icons.
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
