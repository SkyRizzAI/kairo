# Mission Control — Quick-Settings Panel

How the Flipper-style Control Center works *now* (Plan 92 — Control Center).

## What the user sees

A quick-settings panel slid in from the **Desktop** by pressing **Up** or **Left**
(`DesktopScreen::onAction` — `firmware/core/src/screens/desktop_screen.cpp:58`,
`Action::Prev` or `Action::AdjustDown`). Back is ignored on the Desktop (it's home),
so the panel is the only thing those keys reach.

The panel is two regions side by side:

- A **square tile grid** — **dark mode**, **wifi**, **lock**, **settings**,
  **restart**, plus one empty slot. Landscape lays them 3×2, portrait 2×3.
- Two **vertical pill sliders** — **brightness** (sun glyph) and **volume**
  (speaker glyph), each a rounded iOS-style bar with the glyph centred on it.

The focused control is drawn as a rounded inverted box (the icon flips colour).
**Activate** fires the focused tile; **Back** pops the panel back to the Desktop.

## How it works

`MissionControlScreen` (`firmware/core/include/nema/screens/mission_control_screen.h`,
`firmware/core/src/screens/mission_control_screen.cpp`) is a `ComponentScreen` that
the Desktop owns as a member and navigates to.

### State sync on entry

`onResume()` (`mission_control_screen.cpp:32`) reads live state so the panel mirrors
reality: brightness from the display driver, volume from config `aether/volume`,
dark mode from `aether::darkMode()`, and the wifi radio's `isEnabled()`. Focus resets
to tile 0.

### Responsive fit/centre layout

`build()` (`mission_control_screen.cpp:114`) computes a precise **square** tile side.
The bar width tracks the tile (`barW = ts*2/5`) so the ratio holds as the screen
shrinks; the square side is solved from the horizontal budget with that folded in:
`ts = 5·budget / (5·tcols + 4)`, then clamped against the vertical budget and a 12px
floor (`mission_control_screen.cpp:132-137`). Tile icons auto-scale to roughly half
the tile (`ts/2 / native`, min 1× — `:144-148`). The grid reflows by orientation:
landscape `Col{ Row{dark,wifi,lock}, Row{setg,rest,spacer} }`, portrait three rows of
two (`:154-159`). The two sliders are sized to the **grid** height (`gridH`), not the
screen, with a pill radius that scales with width (`:163-169`). The root is a centred
row of `{ grid, brightnessBar, volumeBar }` (`:171-173`).

### Custom 2D grid navigation

`onAction()` (`mission_control_screen.cpp:68`) implements 2D movement over a focus
model where the two bars (indices 5, 6) are repeated in **every** row so Left/Right
always reach them. The grids are static tables — `gLand[2][5]` and `gPort[3][4]`
(`:83-84`), with `-1` marking the empty slot. Left/Right (`AdjustDown`/`AdjustUp`)
scan horizontally to the next valid cell; Up/Down (`Prev`/`Next`) scan vertically
(`:95-98`). **Special case:** when focus is on a bar, Up/Down *adjusts* the slider
instead of moving (`:77-79`) via `adjustBar()` (brightness ±16, volume ±5, clamped —
`:55-66`). `Activate` calls `activate(f)` which dispatches to the tile handler
(`:44-53`); bars have no activate.

### Tile actions

| Tile | Handler | Effect |
|---|---|---|
| Dark mode | `onDark` (`:177`) | Toggles `aether::setDarkMode`, persists config `aether/dark`. |
| WiFi | `onWifi` (`:185`) | Toggles `IWifiDriver` enabled on a `rt.tasks().submit` worker (scan + `autoConnect` on enable). If `savedCount()==0`, enables the radio and **navigates to WiFi settings** so the user can pick a network. |
| Lock | `onLock` (`:204`) | `goBack()` to the Desktop first, then `dpm().lockNow()`. |
| Settings | `onSettings` (`:210`) | Navigates to an owned `SettingsScreen`. |
| Restart | `onRestart` (`:215`) | Navigates to an owned restart-mode `SplashScreen`, which shows for ~2s then calls `requestRestart` itself (aether-side, no kernel hook). |
| Brightness | `onBrightness` (`:222`) | `IDisplayDriver::setBrightness`, persists `display/brightness`. |
| Volume | `onVolume` (`:228`) | Scales every `AudioService` output via `setVolume(v/100)`, persists `aether/volume`. |

### Rendering details

- The vertical sliders draw a rounded outline + bottom-up rounded fill, then overlay
  the glyph by XOR (`invertRect` per set pixel) so it reads on both fill and track
  (`firmware/core/src/ui/renderer.cpp:133-154`).
- Tiles use `drawRoundRect`/`fillRoundRect` with `Style.cornerRadius` for the chamfer
  (`renderer.cpp:53-57`). The focus highlight (`highlightBox`, `renderer.cpp:24-46`)
  honours `cornerRadius`: it fills a rounded inverted box matching the border shape, so
  the focused icon flips to the opposite colour.

### Backend caveats

- **SkyRizz E32 backlight is GPIO on/off, not PWM** — `LcdDriver::setBrightness` maps
  any level `>0` to backlight-on (`firmware/boards/skyrizz-e32/src/lcd_driver.cpp:60`),
  so the brightness slider is effectively on/off there.
- **Volume** scales the `I2sSpeaker` tone amplitude — the *only* playback path on that
  board (`firmware/boards/skyrizz-e32/include/nema/skyrizze32/i2s_speaker.h:21-23`).
