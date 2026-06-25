# Colour Themes & Dark Mode

> Plan 92 Fase B — recolour the whole UI by swapping a 2-colour palette over the
> 1-bit framebuffer. Colour is orthogonal to size/spacing; dark mode is a fg/bg
> swap. Gated on whether the panel can actually show colour.

## Overview

The Aether UI renders into a 1-bit monochrome framebuffer (1 = ink, 0 = paper).
A **colour theme** is just a pair of RGB565 values — `fg` (ink) and `bg` (paper) —
that the display driver expands the framebuffer into when it pushes pixels. So the
entire UI is recoloured by changing two numbers; no screen code is touched. This is
deliberately **orthogonal to the size `StyleTokens`** (spacing, fonts, icons): a
theme swap changes colours only, never layout.

Built-ins: **mono** (white ink on black paper — the default) and **flipper** (black
ink on Flipper-orange paper). **Dark mode** swaps fg/bg. On a true B&W panel (e-ink)
the driver ignores the colours, so only on/off matters and the Theme selector is
hidden — dark mode there is effectively an invert.

## What the user sees

- **Settings → Appearances → Theme** cycles `default` (mono) / `flipper`. This row
  only appears on a colour-capable panel; e-ink boards don't show it.
- **Settings → Appearances → Dark Mode** toggles `On`/`Off` on every board.
- Changes apply on the next frame and persist (config `aether/theme`, `aether/dark`),
  so they survive reboot.
- The Forge simulator and the live remote-desktop mirror follow the **device's**
  Theme setting — they recolour to match Settings, not a separate web picker.

## How it works

### Colour tokens (core, hardware-agnostic)

`ColorTokens { name, fg, bg }` lives alongside but separate from the size
`StyleTokens`. Built-ins and the active-palette accessors are in
`firmware/core/include/nema/ui/style_tokens.h:76` and
`firmware/core/src/ui/style_tokens.cpp:55`:

- `monoColors()` = `{ "mono", 0xFFFF, 0x0000 }` — white ink, black paper.
- `flipperColors()` = `{ "flipper", 0x0000, 0xFB20 }` — black ink, orange paper.
- `colorTheme()` / `setColorTheme()` hold the active palette (mono if unset).
- `darkMode()` / `setDarkMode()` — when true, fg/bg are swapped at push time.

### Pushing the palette each frame

`AetherServer::renderFrame` reads the active palette, applies the dark-mode swap, and
pushes it to the driver every frame (`firmware/core/src/ui/aether_server.cpp:27`):

```cpp
uint16_t fg = aether::colorTheme().fg, bg = aether::colorTheme().bg;
if (aether::darkMode()) { uint16_t t = fg; fg = bg; bg = t; }
c.driver().setPalette(fg, bg);
```

### Display HAL seam

`IDisplayDriver` exposes two seams (`firmware/core/include/nema/hal/display.h:59`,
`:72`): `setPalette(fg, bg)` (default no-op — a B&W panel ignores it) and
`supportsColor()` (default `false`). Both are overridden per board.

### Board driver — skyrizz ILI9341

`LcdDriver` reports `supportsColor() == true` (RGB565 panel) and **byte-swaps** the
RGB565 values in `setPalette` (`firmware/boards/skyrizz-e32/include/nema/skyrizze32/lcd_driver.h:54`):

```cpp
void setPalette(uint16_t fg, uint16_t bg) override {
    auto bswap = [](uint16_t c){ return (uint16_t)((c >> 8) | (c << 8)); };
    setColors(bswap(fg), bswap(bg));
}
```

The chunk buffer is sent little-endian but the panel reads each pixel big-endian, so
without the swap orange `0xFB20` arrives as `0x20FB` and shows blue/green. Symmetric
colours (white `0xFFFF` / black `0x0000`) are swap-invariant, which is why mono looked
fine and only flipper exposed the bug.

### Boot — load from config

`bootDisplay` reads config at startup (`firmware/aether/src/boot_display.cpp:36`):
`aether/theme` selects the colour theme (`flipper` → `flipperColors`, else
`monoColors`) and `aether/dark` (int, non-zero) sets dark mode.

### Settings screen — gated selector

`AppearancesSettingsScreen` builds the Theme row only when the driver is colour-capable
(`firmware/core/src/screens/appearances_settings_screen.cpp:189`):

```cpp
bool color = rt_.canvas().driver().supportsColor();
... color ? input("Theme", kThemeNames[themeIdx_], themeAdj) : nullptr,
```

`applyTheme` (`:43`) sets the colour theme + writes `aether/theme`; `toggleDark` (`:56`)
calls `setDarkMode` + writes `aether/dark`. Both `requestRedraw()`; the palette lands
on the next `renderFrame`. The Dark Mode row is always present (works on B&W too).

### Device → Forge mirror

`RemoteScreenTap` wraps the real driver. On a palette change it forwards to the inner
driver and emits a one-shot **System-channel** message
(`firmware/core/src/hal/remote_screen_tap.cpp:86`): `SysOp::SetPalette` (`0x03`),
payload `[op][fg:2 LE][bg:2 LE]` RGB565. (Screen frames themselves stay 1-bit RLE on
the Screen channel — only the palette colours travel here.)

On the host, `packages/link/src/session.ts:387` decodes `SET_PALETTE`, converts each
value via `rgb565ToRgb888` (`packages/link/src/codec.ts:158`), and emits a `palette`
event (`PaletteInfo { fg, bg }`). `simStore` subscribes and stores it
(`packages/forge/src/lib/simStore.svelte.ts:39`), and the simulator page feeds it to
`BoardVisual` (`packages/forge/src/routes/simulator/+page.svelte:304`):
`on={simStore.palette?.fg ?? THEMES[theme].fg}` — the device palette wins, with a
local theme preset only as a pre-boot fallback.

## File reference

| File | Role |
|---|---|
| `firmware/core/include/nema/ui/style_tokens.h` / `src/ui/style_tokens.cpp` | `ColorTokens`, mono/flipper built-ins, `colorTheme`/`darkMode` state |
| `firmware/core/include/nema/hal/display.h` | `setPalette` + `supportsColor` HAL seams |
| `firmware/boards/skyrizz-e32/include/nema/skyrizze32/lcd_driver.h` | ILI9341 `setPalette` (BGR byte-swap), `supportsColor() = true` |
| `firmware/core/src/ui/aether_server.cpp` | `renderFrame` pushes the dark-swapped palette every frame |
| `firmware/aether/src/boot_display.cpp` | loads `aether/theme` + `aether/dark` at boot |
| `firmware/core/src/screens/appearances_settings_screen.cpp` | Theme (colour-gated) + Dark Mode selectors |
| `firmware/core/src/hal/remote_screen_tap.cpp` | `SetPalette` (0x03) over the PLP System channel |
| `packages/link/src/session.ts` / `codec.ts` | decode `SET_PALETTE`, `rgb565ToRgb888`, `palette` event |
| `packages/forge/src/lib/simStore.svelte.ts` / `routes/simulator/+page.svelte` | Forge applies the device palette to `BoardVisual` |
