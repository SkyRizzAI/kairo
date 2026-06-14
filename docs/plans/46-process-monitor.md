# 46 — Process Monitor (`ps`)

> A `top`/btop-style snapshot over the CLI: list the live **services** (with
> state), **running apps** (foreground/paused), and **CLI sessions** (id + cwd) —
> the "real OS" introspection. Pure read-over-existing-state: ServiceManager
> already tracks service states, AppHostManager owns the running apps, and the
> CliSessionManager (Plan 45) owns the sessions.

- Status: ✅ Done & build-verified (host 10/10 + ESP32 dev-board + WASM green).
- Milestone: M12 (Runtime Foundation)
- Depends on: Plan 05 (ServiceManager), Plan 22 (AppHostManager), Plan 45 (sessions)

---

## What it shows

`ps` (one command):
```
SERVICES:
  GuiService      [running]
  RemoteService   [running]
  ...             [running|stopped|failed|…]
APPS:
  Clock   [foreground]
  WiFi    [paused]
SESSIONS:
  #1  cwd=/apps
  #2  cwd=/
```

## Implementation (no new state — just expose what exists)

- `Runtime::serviceState(IService*)` → forwards to `ServiceManager::stateOf`
  (guarded; `Created` before start). Services enumerated via `container().services()`.
- `AppHostManager::hasForeground()` + `foregroundName()` (mirror of the existing
  `hasPaused()`/`pausedName()`).
- `ps` command in `registerCoreCliCommands`: services + apps + sessions.
- `serviceStateStr()` maps `ServiceState` → text.

## Non-goal

- CPU%/memory-per-process (no per-thread accounting on the kernel yet), live
  refresh/TUI (it's a snapshot; re-run for fresh state), kill-by-name.
