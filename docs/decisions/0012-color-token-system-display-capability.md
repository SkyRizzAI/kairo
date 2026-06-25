# 12 — Color token system & display-capability rendering

- **Status:** Accepted (2-colour palette path shipped; the multi-colour `Color`/`toMono`
  tier remains future work)
- **Date:** 2026-06-25
- **Area:** ui/theme, core/display (Canvas), board/display-driver
- Refines: [ADR 0002](0002-nema-aether-boundary.md) (nema ↔ aether boundary),
  [ADR 0003](0003-nema-display-shared-primitive-layer.md) (shared display primitives),
  [ADR 0004](0004-shell-desktop-launcher-skins.md) (skins)
- Implemented by: [Plan 92](../plans/92-color-system-rotation.md)

## Context

The Aether UI renders entirely in 1-bit: `Canvas` primitives take a `bool on`
(`drawPixel(x,y,bool)`, `fillRect(...,bool)`), and the SkyRizz-E32 panel is monochrome
(foreground = white, background = black). The theme layer (`StyleTokens`, ADR-adjacent,
Plan 53) currently encodes **spacing / icon-size / font-scale / status-bar fill** — but
**no colour**.

We want themes to be primarily about **colour**, not size:

- A **monochrome default** theme (white-on-black, today's look).
- A **Flipper** theme: foreground black, background orange.
- **Dark mode per theme**: Flipper dark = the inverse (background black, foreground
  orange).

Palanu is board-agnostic ("check capabilities, never board type" — CLAUDE.md). So the
system must let a *colour* theme render real orange on an RGB panel **and** degrade to a
sensible on/off image on a 1-bit panel — without any screen, widget, or theme branching
on board identity. The question this ADR settles is **where colour is defined, where the
mono conversion happens, and what the `Canvas` colour contract is.**

Relevant existing fact: `Canvas` already exposes `supportsRgb565()` + `blitRgb565()`
(used by the camera). So the notion of a driver declaring a colour capability already
exists in a narrow form; this ADR generalises it to the whole render path.

## Decision

Introduce a **semantic colour-token layer in the theme** plus a **capability-driven
resolve at the Canvas/driver boundary**. Responsibility is split across three layers; the
board profile defines **no** colour.

### 1. Colour is a theme concern — semantic tokens carrying RGB565 + a mono channel

A new `Color` value and `ColorTokens` set live in the **theme** (`aether::`, core,
board-agnostic):

```cpp
struct Color {
    uint16_t rgb565;          // the colour on an RGB panel
    uint8_t  monoOverride;    // 0 = auto, 1 = force OFF, 2 = force ON  (see §3)
};

struct ColorTokens {
    Color bg;        // surface background
    Color fg;        // primary text/ink on bg
    Color accent;    // selection / highlight fill
    Color accentFg;  // ink on top of accent
    Color muted;     // hairline borders, drop-shadow, dividers
};
```

Themes are **semantic**: ListView draws rows as `fg`-on-`bg`; the selected row draws
`accentFg`-on-`accent`. The same code yields white-on-black (mono), black-on-orange
(Flipper), etc. — no per-theme widget code.

### 2. The internal colour space is RGB565

16-bit is the native format of the target panels (ST7789/ILI9341), matches the existing
`blitRgb565()` path, and halves framebuffer memory vs RGB888. RGB888 was rejected as
over-precise for these panels (it would be down-converted to 565 anyway).

### 3. Mono conversion is **hybrid: auto-luminance with explicit override**

When the active display reports a **monochrome** pixel format, each `Color` resolves to a
single on/off bit:

- **Default (auto):** threshold the RGB565 luminance. This "just works" for high-contrast
  2-colour themes (Flipper orange ≈ luminance 173 → ON/white; black → OFF) without the
  theme author filling anything in.
- **Override:** `monoOverride` (1 = force OFF, 2 = force ON) lets the theme author pin a
  colour's 1-bit result when auto would lose contrast (two tokens of similar luminance).
  This keeps the system pixel-perfect by intent, not by luck.

The resolve is a pure function `bool toMono(Color)` evaluated at the **Canvas/driver
boundary**, not in widget code.

### 4. The display driver declares capability and does the final conversion

The **display driver** (board layer) declares its pixel format — `monochrome` or
`rgb565` — surfaced as a capability (e.g. `display.color`) generalising the existing
`Canvas::supportsRgb565()`. The `Canvas` colour path:

- On an **RGB** display: writes the `rgb565` value.
- On a **monochrome** display: writes `toMono(color)`.

`Canvas` gains colour-typed primitives (`drawPixel(x,y,Color)`, `fillRect(...,Color)`,
`drawText(...,Color)`, …). The legacy `bool on` overloads are **retained** and map to
`on ? fg : bg` of the active theme, so the migration is incremental and no call site is
forced to change at once.

### 5. Board profile only wires the driver

The board profile selects **which** display driver is installed; the driver inherently
carries the colour capability. The board profile never defines colours. This preserves
"capabilities, never board type" — core/theme/widget code asks the capability, never the
board name.

### Layer summary

```
Theme (aether, board-agnostic)   → ColorTokens (RGB565 + mono channel), SelectionStyle
        │ resolve by capability
Canvas + renderer (nema core)    → Color type; picks rgb565 or toMono() per driver cap
Display driver (board)           → declares monochrome|rgb565; final pixel write; rotation
Board profile                    → only selects the driver (no colour)
```

This sits cleanly under ADR 0002/0003: the **`Color` type + Canvas colour API are core
primitives** (`nema::display`, every server needs them, like fonts), while **`ColorTokens`,
`SelectionStyle`, and named themes are presentation** (`aether::`).

## Consequences

- **Themes become colour-first** (mono default, Flipper, …) while still allowed to tune
  size tokens. Dark mode is a second `ColorTokens` set per theme + a global toggle
  (config `display/dark_mode`); the mono channel resolves both modes correctly on 1-bit.
- **Selection style becomes themeable** (`SelectionStyle = Invert | FillRounded |
  DropShadow`). `DropShadow` (Wii-style 1px offset in `muted`) is **colour-only**;
  monochrome displays fall back to `Invert`. This resolves an open question from
  `ui-design-system.md` §15.
- **Board-agnostic, by capability.** Adding an RGB board needs no theme/widget change; the
  driver just declares `rgb565`. Adding a mono board needs no theme change; auto/override
  handles degradation.
- **Cost: a large, load-bearing Canvas migration.** `bool on` → `Color` touches
  `canvas.h`, the renderer, `draw.h`, and every widget. Mitigated by keeping the `bool`
  overloads (→ fg/bg) so migration is incremental, but until fully migrated, two colour
  idioms coexist. This must be phased (Plan 92) and is the main risk.
- **Mono override is a manual quality gate.** Auto-luminance is the default; a theme that
  looks muddy on 1-bit is fixed by setting `monoOverride`, not by changing widgets.
- **Display rotation rides on the same driver-capability seam** (Plan 92 Fase A): rotation
  (0/90/180/270) is a driver concern (HW `MADCTL` preferred, software fallback in
  `flush()`); because the UI is resolution-independent, rotation only swaps
  `canvas.width()/height()` and the layout reflows. **Touch coordinates must apply the
  same transform** or input desyncs — this is the known trap.
- **Ruled out:** defining colours in the board profile (breaks board-agnosticism);
  RGB888 internal (over-precise); pure auto-luminance with no override (fragile for
  near-luminance pairs); pure explicit mono bit with no auto (burdens every theme author).
