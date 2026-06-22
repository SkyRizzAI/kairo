# App Model — Process-First (Plan 86)

## Mental model

Every Palanu app is a **process**. Default = terminal/CLI. GUI is opt-in via API call.

Analogy: like Node.js. An app written as a CLI program runs in a terminal. If it
calls `require('electron')`, it opens a window. The launcher doesn't decide —
**the app decides at runtime**.

```
launch(id, argv)               // from icon OR CLI `run <id> args…`
   │
   ▼
AppHost  ──  app thread → main(argc, argv)
   │
   ├── Mode::Terminal  (DEFAULT)
   │     stdout/stderr → terminal screen on device
   │     "Press any key to exit" when main() returns
   │
   └── Mode::Gui  (triggered by first canvas_* or ui_* call)
         app owns the canvas/event loop
         host stops auto-rendering terminal
```

Transition rule: host starts in **Terminal** mode. First call to any `canvas.*` or
`ui.*` import flips to **Gui** mode (irreversible for that run).

## Manifest schema

```jsonc
{
  "id":      "com.example.counter",   // REQUIRED
  "name":    "Counter",               // REQUIRED
  "version": "1.0.0",                 // REQUIRED
  "entry":   "counter.wasm",          // entry file
  "runtime": "wasm",                  // "wasm" | "js"
  "args":    ["--ui"],                // default argv when launched from icon
  "icon":    "icon.raw",              // optional
  "category": "Demo"                  // optional
  // "mode" field is IGNORED — apps decide CLI vs GUI at runtime
}
```

`args` works like the `Exec=` args in a Linux `.desktop` shortcut:
- Launch from icon → `argv = [id] + manifest.args`
- Launch from CLI (`run counter inc`) → `argv` = what user typed

## ABI surface (WASM)

### Terminal (default)
App uses `nema_print` / WASI `fd_write(stdout)` → output captured and shown on
terminal screen. No ABI call needed for this mode.

### Raw canvas (`canvas.*` module — Plan 86 Fase 2)

```c
canvas_width()  → i32          // logical pixels
canvas_height() → i32
canvas_clear(color)             // COLOR_BLACK=0, COLOR_WHITE=1
canvas_pixel(x, y, color)
canvas_fill_rect(x, y, w, h, color)
canvas_rect(x, y, w, h, color)
canvas_line(x0, y0, x1, y1, color)
canvas_text(x, y, msg_ptr, color)
canvas_flush()                  // present() + enter Gui mode
```

First `canvas.*` call flips host to Gui mode. `canvas_flush()` = `present()`.

### Retained UI (`ui.*` module — Plan 86 Fase 3)

```c
ui_begin()
ui_title(msg_ptr)
ui_text(msg_ptr)
ui_button(label_ptr, id)        // id > 0, returned by ui_wait_event on Activate
ui_row_begin() / ui_row_end()
ui_col_begin() / ui_col_end()
ui_end()                        // commit + render frame
ui_wait_event()  → i32         // block; returns widget id or EV_BACK=-1
ui_poll_event()  → i32         // non-blocking; EV_NONE=0
```

App holds the event loop itself (`while(1) { ui_begin()…ui_end(); ev=ui_wait_event(); }`).
No host→guest callbacks. Host only returns an integer event id.

### Input & timing (`input.*` module — Plan 86 Fase 4)

```c
input_poll()           → i32   // ACT_NONE=0 if empty
input_wait(timeout_ms) → i32   // blocks
delay(ms)                      // yield app thread
```

Actions: `ACT_NONE=0`, `ACT_PREV=1`, `ACT_NEXT=2`, `ACT_ACTIVATE=3`, `ACT_BACK=4`,
`ACT_UP=5` (AdjustUp), `ACT_DOWN=6` (AdjustDown).
Up/Down/Left/Right d-pad keys are mapped to Prev/Next by the board keymap.

## SDK header (Plan 86 Fase 5)

Single include: `#include "nema_api.h"`. Provides `printf` shim (bare-metal, no
WASI libc), all `canvas_*`/`ui_*`/`input_*` declarations, and `COLOR_*` constants.

## Example — minimal app

```c
// Terminal app — no UI. Shows on terminal screen.
#include "nema_api.h"
NEMA_EXPORT int main(void) {
    char name[64] = "World";
    if (nema_argc() > 1) nema_argv_get(1, name, sizeof(name));
    printf("Hello, %s!\n", name);
    return 0;
}
```

```c
// Canvas app — raw pixel drawing. First canvas_flush() flips to Gui mode.
#include "nema_api.h"
NEMA_EXPORT int main(void) {
    int w = canvas_width(), h = canvas_height();
    canvas_clear(COLOR_BG);
    canvas_text(2, 2, "Hello Palanu!", COLOR_FG);
    canvas_flush();
    while (input_wait(0) != ACT_BACK) {}
    return 0;
}
```

```c
// Retained-UI app — declarative widgets, focus navigation.
#include "nema_api.h"
NEMA_EXPORT int main(void) {
    int count = 0;
    while (1) {
        ui_begin();
        ui_title("Counter");
        ui_button("+1", 1);
        ui_button("-1", 2);
        ui_end();
        int ev = ui_wait_event();
        if (ev == EV_BACK) break;
        if (ev == 1) count++;
        if (ev == 2) count--;
    }
    return 0;
}
```

## JS/TSX apps — convergence path (Plan 86 Fase 8)

Existing JS/TSX apps **continue to work unchanged** — no breaking changes.
Under the hood they already follow the process-first model:

| JS/TSX concept | Process-first mapping |
|---|---|
| `class MyApp extends ComponentApp` | a process whose `main()` calls `ui_begin()/ui_end()` equivalent |
| `build(arena, rt) → UiNode*` | the retained-UI frame declaraction |
| `onAction(action)` | the event dispatch (equivalent to `ui_wait_event()` return) |
| `AppHost` running `run()` loop | the host-side event loop, same as `wasm_ui.cpp` does for WASM |

The north-star: a future JS API would be `const ui = require('nema/ui'); ui.begin()...`
instead of TSX class syntax, mirroring the WASM ABI exactly. Until then, the TSX
`ComponentApp` pattern remains the idiomatic way to write JS apps.

**No regression guarantee:** the `AppHost` and `ComponentApp` paths are unchanged.
Any app that built before Plan 86 continues to build and run.

## What changed from the old model

| Old (mode-based) | New (process-first) |
|---|---|
| Manifest declared `"mode":"cli"\|"ui"` | Manifest has no `mode`; field ignored |
| Launcher routed Cli → headless, Ui → AppHost | Launcher always uses AppHost |
| WASM `main(int argc, char* argv[])` with dead args | `NEMA_EXPORT int main(void)` + `nema_argc()`/`nema_argv_get()` |
| WASM apps ran synchronously in `onStart()` | WASM `main()` runs on app thread (Fase 1) |
| No canvas/UI ABI for WASM | `canvas.*`, `ui.*`, `input.*` imports (Fase 2–4) |
| No install flow for WASM bundles | `.papp.zip` upload in Forge → auto-unpack → AppScan (Fase 6) |
