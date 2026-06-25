# Footer Legends

A bottom soft-key / hint bar for Aether screens: a row of filled pill buttons,
each a small icon + short caption (the Flipper-style "Mission" / "Launcher"
legends). It is a Layer-3 Aether widget — built from primitives (`Row`,
`Pressable`/`View`, `Icon`, `Text`), laid out by the flexbox engine, painted by
the renderer. No custom drawing in screen code.

`aether::ui::FooterLegends(NodeArena&, const LegendItem* items, int count)`
→ `firmware/core/include/nema/ui/widgets.h`,
   `firmware/core/src/ui/widgets.cpp`.

## Layout (Phase 1)

The container is a `Row` whose `justify` is chosen from the item count:

| Items | Justify          | Result                                   |
|-------|------------------|------------------------------------------|
| 1     | `Start`          | `[item]` — hugs the **left** edge        |
| 2     | `SpaceBetween`   | `[item]            [item]` — both edges  |
| 3+    | `SpaceBetween`   | `[a]      [b]      [c]` — ends pinned, middle centred |

Because it relies on `Justify::SpaceBetween`, **the row needs full-width slack**:
place it inside a `Align::Stretch` parent (the normal bottom-bar case) or set the
returned node's `style.width` to the canvas width. With no slack, space-between
has nothing to distribute and items collapse to the left.

For 3 (or any odd count of equal pills) the equal gaps on both sides keep the
middle pill centred on the canvas centre — alignment is a free consequence of the
flex engine, not special-cased.

## Pills

Each item is one pill:

- A filled rounded capsule (`background = true`, `cornerRadius = 1`), kept
  **compact** — 1px padding, 1px icon/label gap, a 6×6 icon (`nav.up` / `nav.enter`)
  and a `Mono` (6×8 pixel) label. Pill height ≈ 10px, close to the status bar, so
  it doesn't dwarf the screen at 2× canvas scale. The pixel label matches the retro
  look better than proportional text. (`Mono` 6×8 is the smallest registered font;
  going smaller would need a custom tiny font.)
- Optional icon + optional caption, both `style.background = true` so they render
  in **paper colour** (inverted) over the dark fill — white-on-dark on the mono
  theme, orange-on-dark on Flipper. Icon inversion is handled by the renderer's
  `NodeType::Icon` branch (mirrors how Text inverts via `on = !s.background`).
- Tappable when `onPress` is set, but **not a focus stop** (`focusable = false`):
  it is a hint strip — the physical key it labels performs the real activation.

## Usage

```cpp
LegendItem items[] = {
    { .icon = findIcon("nav.up")->bitmap,     .iconW = 8, .iconH = 8,
      .label = "Mission Control" },
    { .icon = findIcon("action.dot")->bitmap, .iconW = 8, .iconH = 8,
      .label = "Launcher" },
};

// Bottom child of a Stretch Col (full width → space-between has slack).
Col(a, stretchCol, {
    /* ...content... */
    FooterLegends(a, items, 2),
});
```

Icons added to the pack for this (`firmware/core/src/ui/icon_pack.cpp`):
`nav.up` (vertically-centred up-chevron), `nav.enter` (↵ return/enter arrow),
`action.dot` (filled disc).

## Applied: DesktopScreen

`DesktopScreen` (`firmware/core/src/screens/desktop_screen.cpp`,
`drawFooterLegends()`) paints the bar over the live wallpaper:

- **Left** — up-chevron + "Mission Control" (Up/Prev opens Mission Control).
  Auto-shortens to **"Missions"** when the canvas is too narrow to fit both pills
  with an edge inset (width measured via `measureTextW`).
- **Right** — ↵ enter icon + "Launcher" (OK/Activate opens the Launcher).

The bar is a pure hint overlay: the physical Up / OK keys do the real navigation
in `onAction()`, so the pills are not focus stops. It is rendered with
`renderComponentFrame()` into the bottom of the content region after the skin
draws, every frame.

## Collapse animation (Phase 2)

After the bar appears it shows full labels, then — once `collapseDelayMs` (2 s)
elapses — each pill animates `[icon label] → [icon]`: the label width tweens to 0
and the pill shrinks to a tight icon, while `SpaceBetween` keeps the row aligned
(3-item: middle stays centred).

The intro **replays every time the screen comes back lit** — returning from the
launcher (via `onResume`) and waking from sleep/lock both reset to icon+label and
re-collapse after the delay. Pure sleep→wake doesn't navigate (no `onResume`), so
`DesktopScreen::tick()` detects the `dpm.isDisplayOff()` off→on edge and re-arms;
the timer is frozen while the display is off so it never collapses unseen.

Mechanics:

- **State** lives in a caller-owned `FooterLegendsState` (outside the arena, like
  `ScrollState`): a wall-clock `elapsedMs` timer + delay/duration. The screen
  re-arms it in `onResume()` and advances it in `tick()`, requesting a redraw only
  during the collapse window.
- **Time-based, not spring.** `reveal(i)` is computed from elapsed milliseconds and
  a fixed eased duration (smoothstep), so the collapse takes the same wall-clock
  time on any frame rate. (An earlier spring version settled in ~0.3 s at 60 FPS on
  WASM but crawled to ~5 s on the low-FPS skyrizz-e32 — a per-frame integrator is
  frame-rate dependent; an elapsed-time tween is not.)
- **No build-time text measurement.** The animated builder sets
  `Style::widthScale = reveal` on the label; the **layout engine** scales the
  label's measured width (`layout.cpp`), so the pill auto-shrinks. The label is
  drawn with `Style::clip = true` so the renderer truncates it cleanly to the
  shrinking box (`renderer.cpp` Text branch). Below ~0.04 reveal the label (and
  its gap) is dropped, leaving a tight `pad + icon + pad` capsule.

`widthScale` + `clip` are general `Style` additions (default 1.0 / false), not
footer-specific — any shrink-and-truncate label can reuse them.

## Tests

`firmware/tests/layout_test.cpp` (host build, `bun run test`):

- `test_footer_legends()` — the three static layouts: 1 item at x=0, 2 items
  flush to both edges, 3 items with the middle pill centred (±1px).
- `test_footer_legends_collapse()` — the collapse timer arms after the delay and
  the reveal settles toward 0; a fully-collapsed pill is icon-only
  (`pad + icon + pad`) with the row's edges still pinned; full reveal keeps the
  label (pill wider than icon-only).

## Status

- **Phase 1 (layout system)** — done. Count-driven space-between, paper-colour
  icon + label pills, applied to `DesktopScreen`.
- **Phase 2 (collapse animation)** — done. Label-width spring collapse to
  icon-only, alignment preserved, driven from the screen tick.
