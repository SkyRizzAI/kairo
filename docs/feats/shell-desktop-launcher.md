# Shell — Desktop + themed Launcher

How the idle screen and the system menu work *now* (Plan 81 / [ADR 0004](../decisions/0004-shell-desktop-launcher-skins.md)).

## What the user sees

- **Desktop** — the boot/idle screen. A full-screen live wallpaper (currently a
  dolphin animation). Pressing **OK** (Activate) opens the Launcher. Back does
  nothing (this is home).
- **Launcher** — the system menu, opened from the Desktop. Its look is a
  selectable *skin*:
  - **PlayStation** (`playsta`) — horizontal carousel: banner title + a large
    center tile with preview tiles either side + a position bar.
  - **Nintendo Wii** (`wii`) — 2-column channel-tile grid with a scrollbar.
  Entries: **Apps** (→ app list), **Files**, **Dolphin**, **Logs**, **Settings**.
  Back returns to the Desktop.

## Where it's configured

Settings → **Display & Appearances** (`SleepSettingsScreen`):

| Row | Config key | Values |
|---|---|---|
| Theme | `display/theme` | `flipper` · `compact` · `large` |
| Desktop | `display/desktop` | `livewal` |
| Desktop Setting › | (sub-screen) | Wallpaper · Fit · Anchor |
| Launcher | `display/launcher` | `playsta` · `wii` |
| Assets Pack | `display/assets` | `palanu` |
| Status Bar | `display/statusbar` | `ON` · `OFF` |

**Desktop Setting** (`DesktopSettingScreen`) edits the wallpaper placement:

| Row | Config key | Values |
|---|---|---|
| Wallpaper | `desktop/wallpaper` | `dolphin` |
| Fit | `desktop/fit` | `center` · `stretch` · `crop` · `fit` |
| Anchor | `desktop/anchor` | 9-grid (`top-left` … `center` … `bot-right`) |

Changes persist immediately; the Desktop/Launcher re-read config on `onResume()`,
so a new skin or fit shows the next time the screen opens.

## How it's built

`nema::shell` separates behaviour (screens) from look (skins):

- **`ILauncherTheme`** (`shell/launcher_theme.h`) — `draw(canvas, model, cursor)` +
  `columns()`. Skins: `PlayStationLauncher`, `WiiLauncher`.
- **`IDesktopTheme`** (`shell/desktop_theme.h`) — `draw(canvas, rect)` + `tick()` +
  `player()`. Skin: `LiveWallpaperDesktop` (reuses a dolphin showcase animation;
  blits each frame with a fit/anchor nearest-neighbour scaler).
- **`shell_factory`** — maps a config name → a concrete skin. One line per new skin.
- **`LauncherScreen`** owns the fixed entry model, the cursor, **linear**
  navigation (±1, board-agnostic), Activate routing, and Back→Desktop.
- **`DesktopScreen`** owns wallpaper lifecycle: builds the skin from config,
  registers/unregisters the animation with `AnimationManager` across resume/pause,
  and runs Normal mode (status bar on, wallpaper fills the content area) or
  Fullscreen (status bar off, edge-to-edge) per `display/statusbar`.

Boot pushes `DesktopScreen` (all three targets: skyrizz-e32, dev-board, wasm).

## Adding a skin

1. Implement `ILauncherTheme` (or `IDesktopTheme`) in a new `shell/…` file.
2. Add one branch in `shell_factory.cpp`.
3. Add the name to the cycle array in `SleepSettingsScreen`.

No changes to `LauncherScreen`/`DesktopScreen` are needed.
