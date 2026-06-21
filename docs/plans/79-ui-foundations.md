# 79 — UI Foundations: proportional fonts, ListView, scrollbar, UI scale

> Flipper-style list UI groundwork: proportional bitmap fonts (regular+bold at
> 8/10/12px), a `ListView` component family, a corrected scrollbar, and a live
> logical-scale (pixelation) control.
>
> - Status: ✅ shipped (host + wasm + esp32 build-verified; flashed on skyrizz-e32).
> - Note: the namespace these live in (`nema::ui`) is migrated to `aether::` by
>   [Plan 80](80-aether-modularization.md).

## What shipped

- **Proportional + tall fonts.** `BitmapFont` gained a per-glyph width table +
  per-glyph byte offsets + `bytesPerCol` (1 for ≤8px, 2 for 9–16px); `Canvas`
  glyph blit + `measureTextW` honour them. `tools/fonts/encode.py --prop` converts
  u8g2 BDFs → proportional C arrays. Generated **Regular + Bold at 8/10/12px**
  (Helvetica), registered as `Fonts::Reg8/Bold8/…`; roles remapped (Body→Reg8,
  Subhead→Bold8, Title→Bold10, BigNum→Bold12).
- **Scrollbar fix.** Thumb size = proportional to `viewport/content` (shrinks as
  content grows, floored at a minimum) and position clamped so the far edge never
  overshoots the track. `scrollbar(c,x,y,size, scrollOffset, viewport, content)`.
- **`fillRoundRect`/`drawRoundRect`** (r=1) on Canvas — the Flipper selection box.
- **ListView (Layer-3 widgets):** `ListContainer`, `ListSection` (bold subheader),
  `ListItemRow` (plain / value / chevron / left-icon), `ListInputRow` (fixed-ratio
  split `label  < value >`, centered value, marquee-on-focus label). Rounded inset
  selection box via `Style::selectBox`; fixed split via `Style::flexZero`
  (flex-basis:0). Settings + Display screens restyled.
- **UI Scale control.** Display → Appearances → *UI Scale* cycles the logical-pixel
  scale (1×…2×) live (via `serverScale`, synced to canvas each frame) and persists
  to config `display/scale`. More scale = chunkier/retro.

## Follow-ups

- Marquee for the **value** side of a split row (label marquee done).
- Apply the ListView family to Wi-Fi / file browser / app-list screens.
- Namespace move to `aether::` — Plan 80.
