# 0007 — `isDisplayOff()` as the render-gate predicate

- **Status:** Accepted
- **Date:** 2026-06-22
- **Area:** core/dpm, core/gui-service

## Context

`DisplayPowerManager` has three states: `Active`, `Sleep`, and `Locked`. Before this
decision the GuiService render loop and animation tick were gated on `isSleeping()`,
which only returns `true` in `State::Sleep`.

The Locked state has two sub-phases:
1. **Display-off** — entered via `enterLocked()` after the sleep timeout expires. The
   hardware backlight is still off (no `display_->wake()` has been called). There is
   no visible content.
2. **Display-on** — entered on the first keypress in Locked. `deliverKey()` calls
   `display_->wake()`, pushes the LockScreen, and sets `lockScreenShown_ = true`.

Because `isSleeping()` is `false` in both sub-phases, the render loop ran continuously
during the display-off Locked phase. Every animation frame advanced the wallpaper and
the canvas was flushed to the LCD controller (ILI9341 GRAM). The ILI9341's GRAM is
persistent — whatever was written last shows immediately when the backlight turns on.
The result: the first keypress turned the backlight on to reveal an animation frame
instead of a blank/lock screen, producing a brief visible flash before the LockScreen
rendered in the next frame.

## Decision

Added `isDisplayOff()` to `DisplayPowerManager`:

```cpp
bool DisplayPowerManager::isDisplayOff() const {
    return state_ == State::Sleep ||
           (state_ == State::Locked && !lockScreenShown_);
}
```

`GuiService::loop()` now uses `isDisplayOff()` (not `isSleeping()`) to gate both:
- the animation tick (`AnimationManager::tickAll`)
- the render branch (`vd.takeRedraw()` + `server->renderFrame(...)`)

The sleep-entry blank-frame flush retains its own `isSleeping() && takeEnteredSleep()`
guard — that path is intentional and must still fire.

`lockScreenShown_` flips to `true` exactly when `deliverKey()` wakes the display, so
`isDisplayOff()` becomes `false` in the same event loop iteration that turns the
backlight on. The LockScreen renders into GRAM before the backlight-on signal reaches
the panel, so the first visible frame is always the LockScreen.

## Consequences

- LCD GRAM is never updated while the backlight is physically off — wake is clean
  (blank → LockScreen with no intermediate animation frame visible).
- Unnecessary SPI transfers and animation CPU cost during the locked-dark phase are
  eliminated.
- `isSleeping()` remains meaningful for the sleep-entry blank-frame path; callers that
  only need to know "display visually sleeping" should keep using it for that purpose.
- Any future state that turns the backlight off (e.g. deep-sleep, display fault) must
  also be reflected in `isDisplayOff()` — callers don't need to change.
