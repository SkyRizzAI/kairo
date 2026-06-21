# 81 — Shell: Desktop (live wallpaper) + themed Launcher

> Split today's combined `HomeScreen` into a two-screen **shell**: an idle
> **Desktop** (full-screen live wallpaper, à la Flipper Zero) and a themed
> **Launcher** (system menu) reachable from it. Desktop has one theme for now
> (`livewal`); the Launcher is **skinnable** with two themes — **PlayStation**
> (horizontal carousel) and **Nintendo Wii** (2-column tile grid) — selectable
> from Display & Appearances.
>
> - Status: 🟡 planned
> - Depends on: [Plan 60](60-aether-ui-rewrite.md) (carousel/ListView),
>   [Plan 71](71-asset-loader-build-pipeline.md) (asset packs / wallpaper),
>   [Plan 79](79-ui-foundations.md) (ListView family),
>   [Plan 80](80-aether-modularization.md) (display-server split).

---

## Why

Right now `HomeScreen` (`firmware/core/src/screens/home_screen.cpp`) does **two
jobs at once**: it is the boot/idle screen *and* the app menu (a DSi-style
carousel). We want to separate those roles and make each role themeable:

- **Desktop** — what you see at rest. A live wallpaper, like the Flipper dolphin.
- **Launcher** — the system menu you open *from* the desktop. Its **look** should
  be swappable (PlayStation XMB row vs. Wii channel grid) without changing what it
  *does*.

Target flow (confirmed):

```
[boot] → Desktop (live wallpaper, idle)
            │ Activate (OK)
            ▼
         Launcher  ── PlayStation │ Wii  (config-selected skin)
            │ Back
            ▼
         Desktop
```

## Design

### Two axes, independent

| Axis | What it skins | Interface | Themes (now) | Config key |
|---|---|---|---|---|
| **Theme** | global widget `StyleTokens` (existing) | — (named tokens) | `flipper`*, `compact`, `large` | `display/theme` |
| **Desktop** | the idle wallpaper screen | `IDesktopTheme` | `livewal` | `display/desktop` (+ `desktop/fit`, `desktop/anchor`, `desktop/wallpaper`) |
| **Launcher** | the system-menu skin | `ILauncherTheme` | `playsta`, `wii` | `display/launcher` |
| **Assets Pack** | icon/wallpaper asset set (Plan 71) | `AssetPackRegistry` | `palanu` | `display/assets` |
| **Status Bar** | top status bar on/off | — (bool gate) | `ON` / `OFF` | `display/statusbar` |

\* *`Theme`'s default value is renamed `default` → `flipper` to match the UI; the
underlying tokens (`aether::defaultTheme()`) are unchanged — it's a display-name +
config-value rename with a back-compat read of the old `"default"` string.*

These are orthogonal: a `flipper` Theme can pair with a `wii` Launcher and the
`livewal` Desktop.

### Shell subsystem (new)

New folder `firmware/core/include/nema/shell/` + `…/src/shell/` (presentation lives
in the `nema` / `aether::ui` namespaces per Plan 80):

- **`shell/launcher_model.h`** — `LauncherEntry { const char* label; const char* icon;
  void(*onActivate)(Runtime&); }` and a builder that returns the fixed system entry
  list (shared by every launcher skin so they always show the same items, only laid
  out differently):

  `Apps ›` (→ `AppListScreen`) · `Files` · `Dolphin TV` · `Logs` · `Settings` ·
  `Bad USB` · `Package`.

- **`shell/launcher_theme.h`** — `ILauncherTheme`:
  ```cpp
  struct ILauncherTheme {
    virtual ~ILauncherTheme() = default;
    virtual const char* name() const = 0;          // "playsta" | "wii"
    virtual int  columns() const = 0;              // 1 = carousel row, 2 = grid
    virtual void draw(Canvas&, const LauncherModel&, int cursor) = 0;
  };
  ```
  Behaviour (cursor, wrap, Activate) stays in `LauncherScreen`; the theme owns
  **layout only**. `columns()` lets the screen do generic 1-D/2-D navigation
  (carousel = 1 column → left/right; grid = 2 columns → up/down/left/right).

- **`shell/desktop_theme.h`** — `IDesktopTheme`:
  ```cpp
  struct IDesktopTheme {
    virtual ~IDesktopTheme() = default;
    virtual const char* name() const = 0;          // "livewal"
    virtual void tick(uint32_t nowMs) {}           // advance animation
    virtual void draw(Canvas&) = 0;                // full-screen wallpaper
  };
  ```

- **`shell/shell_factory.{h,cpp}`** — `makeDesktop(name)` / `makeLauncher(name)`
  return a `std::unique_ptr` of the matching impl (default-fallback on unknown
  name). The only place that knows the concrete classes.

### Screens (thin, behaviour-owning)

- **`DesktopScreen`** (`ComponentScreen`, `ScreenMode::Fullscreen`): builds its
  `IDesktopTheme` from config in `onResume()`; `draw()` forwards to the theme;
  `onAction(Activate)` → `rt_.view().navigate(launcher_)`. Registers the wallpaper
  animation player with `AnimationManager` so it ticks.
- **`LauncherScreen`** (`ComponentScreen`): builds `LauncherModel` + its
  `ILauncherTheme` from config in `onResume()`; owns `cursor_`; `onAction` does
  generic grid nav using `theme_->columns()`; `Activate` runs the entry callback;
  `onBackPressed()` returns to Desktop. `draw()` forwards to `theme_->draw(...)`.

### Launcher skins

- **`PlayStationLauncher`** (`playsta`, `columns()==1`): **port the existing
  `HomeScreen::draw()` carousel** (banner + center tile + side previews + posbar)
  almost verbatim — that artwork already matches the PlayStation reference shot.
  Title becomes the device name ("My Palamon"/"My Palanu" — read from config/profile,
  fallback "PALANU").
- **`WiiLauncher`** (`wii`, `columns()==2`): 2-column grid of channel-style tiles
  (icon centered, label under), focused tile inverted/outlined, vertical scrollbar
  on the right. Resolution-independent: tile size from `canvas.width()`/`height()`.

### Desktop skin

- **`LiveWallpaperDesktop`** (`livewal`): draws a full-screen animated 1-bit
  wallpaper. **For now it reuses an existing dolphin animation** (Plan 71
  `dolphin_anim` / built-in `AnimationPlayer`) as the wallpaper source — no new art
  needs converting. Later it can point at an `AnimAsset` from the selected pack
  (`desktop/wallpaper`). The wallpaper bitmap is usually smaller than the screen, so
  the skin honours **fit mode** + **anchor** (see below) when placing each frame.
  Status bar overlays on top when enabled.

#### Wallpaper fit + anchor (the "Desktop Setting" config)

The wallpaper frame rarely matches the panel size, so the desktop is configurable:

- **Fit mode** (`desktop/fit`): how the frame fills the screen.
  - `center` — no scaling; place at native size by anchor.
  - `stretch` — scale X/Y independently to fill (aspect ignored).
  - `crop` (cover) — uniform scale to cover the screen, overflow clipped.
  - `fit` (contain) — uniform scale to fit inside, letterboxed.
- **Anchor** (`desktop/anchor`): 9-grid placement used by `center` (and to bias
  the visible region for `crop`, the letterbox gaps for `fit`): `top-left`,
  `top`, `top-right`, `left`, `center`, `right`, `bottom-left`, `bottom`,
  `bottom-right`.

Scaling is nearest-neighbour over the 1-bit frame (keeps the retro look and is
cheap). A small `Canvas` helper `drawBitmapFit(frame, w,h, fit, anchor)` (or a
shell-local blit) does the placement so both desktop and any future tiled
wallpaper share it.

### Settings — Display & Appearances

Extend `SleepSettingsScreen` (the existing Display screen) — Appearances section
gains rows to match the reference shot:

```
Display
  Sleep After        < 15s  >
  Lock Screen After  < 30s  >
  Debug FPS          < OFF  >
Appearances
  Theme              < flipper >     (rename: was "default")
  Desktop            < livewal >     cycle row  → display/desktop
  Desktop Setting    >               nav row    → push DesktopSettingScreen (its own sub-screen)
  Launcher           < playsta >     cycle display/launcher (playsta | wii)
  Assets Pack        < palanu  >     cycle display/assets (Plan 71 packs)
  Status Bar         < ON >          toggle display/statusbar
```

Each selector cycles → persists to config → `requestRedraw()`. **No live
cross-screen poking**: Desktop/Launcher re-read their config in `onResume()`, so the
new skin shows next time you open them. (Class may be renamed
`SleepSettingsScreen` → `DisplaySettingsScreen` for clarity — optional.)

### Status bar gate

`GuiService` already draws the status bar each frame. Gate it behind
`display/statusbar` (default `ON`); `DesktopScreen`/`LauncherScreen` respect the
same flag. Fullscreen desktop already suppresses it — the toggle controls the
non-fullscreen case + lets the desktop opt back in.

### Boot change

`firmware/targets/*/main/main.cpp`: replace
`static nema::HomeScreen hs(rt); rt.view().push(hs);`
with `static nema::DesktopScreen ds(rt); rt.view().push(ds);`. `HomeScreen` is
retired (its carousel lives on inside `PlayStationLauncher`).

---

## Tasks

### Phase 1 — Shell scaffolding
- [ ] Create `nema/shell/launcher_model.h` — `LauncherEntry` + fixed system entry builder.
- [ ] Create `nema/shell/launcher_theme.h` (`ILauncherTheme`) and `desktop_theme.h` (`IDesktopTheme`).
- [ ] Create `shell/shell_factory.{h,cpp}` — `makeDesktop`/`makeLauncher` (default-fallback).
- [ ] Define config keys + defaults: `display/desktop=livewal`, `display/launcher=playsta`, `display/assets=palanu`, `display/statusbar=1`.

### Phase 2 — Desktop
- [ ] `drawBitmapFit(frame,w,h,fit,anchor)` blit helper — nearest-neighbour scale + 9-grid anchor (center/stretch/crop/fit).
- [ ] `LiveWallpaperDesktop` skin (`livewal`) — reuse an existing dolphin animation as wallpaper source; draw via `drawBitmapFit` + `tick()`.
- [ ] `DesktopScreen` (Fullscreen) — build theme from config, forward draw/tick, `Activate`→Launcher.
- [ ] Register wallpaper animation player with `AnimationManager`.

### Phase 3 — Launcher
- [ ] `LauncherScreen` — model + cursor + generic grid nav via `columns()`; `Activate` runs entry; Back→Desktop.
- [ ] `PlayStationLauncher` (`playsta`) — port `HomeScreen::draw()` carousel; title from device name.
- [ ] `WiiLauncher` (`wii`) — 2-column channel-tile grid + scrollbar, resolution-independent.
- [ ] Wire `Apps ›` entry → `AppListScreen`; verify Files/Dolphin/Logs/Settings/BadUSB/Package route correctly.

### Phase 4 — Settings (Display & Appearances)
- [ ] Rename Theme value `default` → `flipper` (display + config), with back-compat read of `"default"`.
- [ ] Add `Desktop`, `Launcher`, `Assets Pack` cycle rows + persistence.
- [ ] Add `Status Bar` toggle row + gate in `GuiService` / shell screens.
- [ ] Add `Desktop Setting ›` as a **navigation row** (chevron, `onPress`→ `rt_.view().navigate(desktopSetting_)`).
- [ ] New `DesktopSettingScreen` (own sub-screen) — cycle rows: Wallpaper pick · `Fit` (center/stretch/crop/fit) · `Anchor` (9-grid), each persisting `desktop/wallpaper` / `desktop/fit` / `desktop/anchor`; changes redraw the desktop on back.

### Phase 5 — Cleanup & wiring
- [ ] Swap boot push `HomeScreen` → `DesktopScreen` in all `targets/*/main.cpp`.
- [ ] Retire `HomeScreen` (delete or keep only if still referenced).
- [ ] Minimal Assets Pack wiring: cycle lists available packs (just `palanu` now), persists, applied at boot.

### Phase 6 — Verify & docs
- [ ] Build green: host (`nema_core`+`aether`), wasm, esp32 (`skyrizz-e32`).
- [ ] Manual check in simulator: boot→desktop, OK→launcher, switch playsta↔wii, back→desktop.
- [ ] Flash + verify on `skyrizz-e32`.
- [ ] Docs: update `STATE.md`; add `docs/feats/shell-desktop-launcher.md`; ADR for the shell/skin split (`IDesktopTheme`/`ILauncherTheme` + config-driven selection); tick this checklist.
- [ ] Conventional commit(s) so the changelog regenerates.

---

## Open questions / follow-ups

- **Device name** for the PlayStation banner ("My Palamon") — source it from a
  profile/config key (`profile/name`?) or hardcode "PALANU" for now.
- **More launcher themes** later (Steam, PSP, …) drop in as new `ILauncherTheme`
  impls + one factory line + one cycle entry — no screen changes. Same for desktop
  wallpapers under one `livewal` theme.
- **Live preview** in settings (apply skin immediately while cycling) is deferred;
  current design applies on next screen open.
