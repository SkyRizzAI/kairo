# Aether UI v1 Backup

Snapshot of the old Aether look-layer before Plan 60 (big-bang total rewrite).
Backed up automatically during Plan 60 Fase 0.

## Contents

- `ui/` — `src/ui/` look-layer sources: `aether_server`, `renderer`, `widgets`, `status_bar`, `components`
- `ui-include/` — `include/nema/ui/` look-layer headers
- `screens/` — all 13 `src/screens/*.cpp`
- `screens-include/` — all `include/nema/screens/*.h`

## What was NOT backed up (tier-0 engine, kept)

`canvas`, `font_5x8`, `node`, `layout`, `component_runtime`, `component_screen`,
`focus`, `hit_test`, `text_style`, `style_tokens`, `surface`, `display_server`,
`ui_constants`, `view_dispatcher`, `text_input`, `virtual_keyboard`, `aether_abi`,
`ui_sdk`, `ascii_board_renderer`, `fbcon_server`.

## Restore

Copy from this directory back into `firmware/core/src/ui/` and
`firmware/core/include/nema/ui/`, then re-add to `NEMA_CORE_SRCS` in
`firmware/core/CMakeLists.txt`.

## Why this was replaced

Plan 60 rewrites the visual layer (tier-1 draw toolkit + tier-2 widgets + renderer
+ all screens) to match Flipper Zero Momentum style + Nintendo DSi carousel.
The tier-0 engine (node-tree, layout, canvas) was kept and reused.
