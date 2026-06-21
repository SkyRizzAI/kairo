# 4. Shell split: Desktop + themed Launcher (behaviour/skin separation)

- Status: accepted
- Date: 2026-06-21
- Plan: [Plan 81](../plans/81-desktop-launcher-shell.md)

## Context

The original `HomeScreen` (Plan 60) did two jobs at once: it was the boot/idle
screen *and* the app menu, hard-wired as a single DSi-style carousel. We wanted
(a) a Flipper-style idle **Desktop** with a live wallpaper, distinct from (b) a
**Launcher** system menu, and we wanted the Launcher's *look* to be swappable
(PlayStation carousel ↔ Nintendo Wii grid) selectable from settings — with room
to add more skins later without touching navigation logic.

The naïve approach (one screen with a `switch(style)` in `draw()`, and style-aware
navigation) couples look and behaviour: every new skin edits the screen, and 2-D
vs 1-D layouts drag board-specific input-axis handling into the screen.

## Decision

Introduce a small **shell** subsystem (`nema::shell`) that separates *behaviour*
(owned by screens) from *look* (owned by swappable skin objects), selected by config.

- **`ILauncherTheme`** — a stateless drawer: `draw(canvas, model, cursor)` +
  `columns()` (a layout hint only). `LauncherScreen` owns the entry model, the
  cursor, navigation, Activate routing, and Back→Desktop. Navigation is **linear**
  (±1 in reading order) on every skin, so it stays board-agnostic; `columns()`
  affects only how a skin lays tiles out, never traversal.
- **`IDesktopTheme`** — `draw(canvas, rect)` + `tick()` + an optional animation
  `player()`. `DesktopScreen` owns lifecycle: it builds the skin from config,
  registers/unregisters the wallpaper animation with `AnimationManager` across
  resume/pause, and chooses Normal vs Fullscreen mode based on the status-bar flag.
- **`shell_factory`** — the single place mapping a config name → a concrete skin
  (`makeDesktop`/`makeLauncher`). Adding a skin = one new class + one factory line.
- **Selection persists** in the `display`/`desktop` config namespaces; screens
  re-read on `onResume()`, so no live cross-screen poking is needed when a skin is
  changed in settings.

Flow: boot → `DesktopScreen` (wallpaper) → Activate → `LauncherScreen` (skinned) →
Back → Desktop. Apps are not top-level launcher items: "Apps" is one entry that
opens `AppListScreen`.

"Theme" (the global `StyleTokens`) stays an orthogonal axis; its default name was
renamed `default`→`flipper` (display + config), with a back-compat read of the old
string. Desktop, Launcher, Assets Pack, and Status Bar are independent selectors.

## Consequences

- New skins are additive: a class implementing the interface + one factory line +
  one cycle entry in settings — zero changes to `LauncherScreen`/`DesktopScreen`.
- The proven `HomeScreen` carousel art was ported verbatim into `PlayStationLauncher`
  (low risk); `HomeScreen` is retired but kept in-tree until callers are gone.
- Linear launcher navigation is board-safe but means the Wii grid's Up/Down move in
  reading order, not strictly visual rows — acceptable for small menus; true 2-D
  grid nav is a deferred follow-up.
- Wallpaper scaling is nearest-neighbour float math per pixel; fine at low animation
  fps but a candidate for fixed-point optimisation if a full-screen high-fps
  wallpaper is added later.
- One more conceptual layer (`nema::shell`) — justified because it is the seam that
  keeps presentation skins out of screen behaviour, mirroring the ADR 0002 spirit.
