# Bug Fixes — 2026-06-11

Three separate bugs fixed this session, all related to the app runtime layer.

---

## Fix 3 — Focus ring disappears after sleep/wake

**Symptom**: After the device wakes from sleep, the focus ring (button highlight) no longer renders. Navigation still works but there is no visual indicator of which item is focused.

**Root cause**: `ComponentScreen` has an `InputModality` field (`state_.modality`) that controls whether the focus ring is drawn. It defaults to `Button` (ring visible) but gets flipped to `Pointer` on touch interactions. When the display power manager (DPM) puts the device to sleep and the user wakes it by pressing a key, `ComponentScreen::enter()` was never called — so the modality was never reset. The component re-entered still in `Pointer` mode, hiding the ring.

**Fix**: Added `ComponentScreen::enter()` override that resets modality to `Button` and requests a redraw.

```cpp
// firmware/core/src/ui/component_screen.cpp
void ComponentScreen::enter() {
    state_.modality = input::InputModality::Button;
    requestRedraw();
}
```

Both `HomeScreen::enter()` and `AppListScreen::enter()` were updated to call through to `ComponentScreen::enter()` instead of manually calling `requestRedraw()`.

**Files changed**: `core/include/palanu/ui/component_screen.h`, `core/src/ui/component_screen.cpp`, `core/src/screens/home_screen.cpp`, `core/src/screens/app_list_screen.cpp`

---

## Fix 4 — Fullscreen apps freeze at UI scale > 1.0

**Symptom**: Opening any fullscreen app (e.g. Stopwatch) at UI scale 1.5× or 1.75× causes the display to freeze entirely. The app is running (hold-OK returns to home with "Continue App"), but no frames appear on screen.

**Root cause**: `AppHost` allocates its render buffer using `canvas.width()` / `canvas.height()`, which return *logical* dimensions (physical ÷ scale). At scale 1.75×, a 264×176 panel gives logical dims of ~150×100. `AppHost::draw()` then called `display_->flushBuffer(readyBuf_, 150, 100)`, which has a guard:
```cpp
if (w != width_ || h != height_) return;  // silent no-op
```
Since 150 ≠ 264, it returned immediately — display never updated. `suppressCanvasFlush()` returned `true` anyway (wrongly), so the canvas path was also skipped. Double no-op → frozen screen.

**Fix**: The fast `flushBuffer` path now checks that the app buffer dimensions match the physical display before using it. At scale > 1.0, it falls back to a per-pixel blit via `c.drawPixel()`. `suppressCanvasFlush()` was updated to match the same condition.

```cpp
// firmware/core/src/app/app_host.cpp
void AppHost::draw(Canvas& c) {
    std::lock_guard<std::mutex> lk(frameMtx_);
    if (!hasFrame_) return;

    // Fast path only valid at scale 1.0 (buffer == physical dims).
    if (app_.fullscreen() && display_ &&
        w_ == display_->width() && h_ == display_->height()) {
        display_->flushBuffer(readyBuf_, w_, h_);
        return;
    }

    // Scaled fallback: blit pixel-by-pixel through the canvas.
    if (app_.fullscreen()) c.clear(false);
    uint16_t top = app_.fullscreen() ? 0 : (uint16_t)(ui::SEP1_Y + 2);
    for (uint16_t y = top; y < h_; y++)
        for (uint16_t x = 0; x < w_; x++)
            c.drawPixel(x, y, readyBuf_[(size_t)y * w_ + x] != 0);
}

bool AppHost::suppressCanvasFlush() const {
    return app_.fullscreen() && display_ != nullptr &&
           w_ == display_->width() && h_ == display_->height();
}
```

**Files changed**: `core/src/app/app_host.cpp`

---

## Fix 5 — JS apps (Sys Info, Counter JS) freeze/hang at any scale

**Symptom**: Launching any JS app logs `[AppHostManager] launch app=Sys Info` and then hangs indefinitely. The device stays alive (DPM still fires, hold-OK works) but the app never starts. No further log output.

**Root cause (primary)**: `JsApp` overrode `stackBytes()` to return 32768 (32 KB) for its FreeRTOS task. Loading a JS app involves two *nested* `JS_Eval(COMPILE_ONLY)` calls on the native C stack:
1. `loadApp()` calls `JS_Eval(app_js, COMPILE_ONLY)` 
2. This triggers `module_loader` for the `palanu` import
3. `module_loader` calls `JS_Eval(PALANU_RUNTIME_JS, COMPILE_ONLY)` — on the same native stack

Each compilation pass consumes ~12–15 KB of native C stack. Two nested = ~30 KB, which overflowed the 32 KB task stack. FreeRTOS stack overflow on ESP32-S3 in check-type 2 (TRACE) mode does not produce a clean crash or restart — it silently corrupts memory, which manifested as a hang.

**Root cause (secondary)**: `JS_SetMaxStackSize(rt_, 256 * 1024)` was set to 256 KB — larger than the entire task stack — so QuickJS's own overflow guard never fired. The native stack silently blew through the task boundary without any error or exception.

**Fix**:

1. Increased `JsApp::stackBytes()` from 32 KB to 64 KB, giving enough headroom for nested compilation plus rendering plus C overhead.

```cpp
// firmware/core/include/palanu/apps/js_app.h
uint32_t stackBytes() const override { return 65536; }
```

2. Lowered `JS_SetMaxStackSize` to 48 KB on-device (guarded by `#ifdef ESP_PLATFORM`). On the 32-bit ESP32, native C frames are roughly half the size of 64-bit host frames, so 48 KB fits within 64 KB once C overhead is subtracted. On the 64-bit host, tests use the QuickJS default (256 KB) since they are not stack-constrained.

```cpp
// firmware/core/src/js/js_engine.cpp
#ifdef ESP_PLATFORM
    JS_SetMaxStackSize(rt_, 48 * 1024);
#endif
```

3. Added diagnostic log checkpoints in `loadApp()` (`compile` → `eval` → `jobs`) so future hangs can be pinpointed to a specific phase rather than showing nothing after launch.

4. Added missing `#include "nema/runtime.h"` and `#include "nema/log/logger.h"` to `js_engine.cpp` — without these the `host_->log()` calls would not compile (incomplete type errors).

**Files changed**: `core/include/palanu/apps/js_app.h`, `core/src/js/js_engine.cpp`

---

## Fix 6 — JS apps fail to load (CORRECTS Fix 5's diagnosis)

**Symptom**: Opening Sys Info / Counter (JS) either freezes the device (blank
screen, OS hangs) or — on the WASM simulator — shows
`JsApp: JS load failed: Maximum call stack size exceeded`.

**Fix 5 was wrong.** It blamed nested `JS_Eval` compilation overflowing the native
stack and tuned ESP-only numbers (task stack 32→64 KB, QuickJS guard 256→48 KB).
That never fixed it because the diagnosis was wrong on three counts, each verified
empirically with host probes:

1. **The cost is QuickJS *module evaluation* (the interpreter), not parsing.**
   Loading the runtime+apps as precompiled QuickJS *bytecode* needs the **same**
   load stack as source (~24 KB on the 64-bit host) — proven by a bytecode
   round-trip test. So "nested compilation" / eager-compile fixes do nothing, and
   precompiling-to-bytecode is a dead end for this bug (also: bytecode was larger).

2. **`thread_host.cpp` silently ignored `stackBytes()`.** On ESP the app runs on a
   FreeRTOS task sized by `stackBytes()`. On host/WASM it ran on a `std::thread`
   with the platform-default stack — so **every `stackBytes()` tweak was a no-op on
   the simulator.** The QuickJS engine ran on an unsized worker stack.

3. **The QuickJS overflow guard was uncoordinated with the real stack.**
   `JS_SetMaxStackSize` was called **only under `#ifdef ESP_PLATFORM`** (48 KB). On
   WASM/host it was left at QuickJS-ng's **1 MB default** — unrelated to the actual
   worker stack — which is why the guard mis-fired on the simulator
   (`Maximum call stack size exceeded`) for apps that actually need ~24 KB, and why
   on ESP an overflow corrupted the FreeRTOS task stack (silent hang) instead of
   throwing a catchable error.

   Why the host tests "passed": `js_render_test` loads an app importing only from
   `"nema"`. The real apps dual-import `"nema"` **and** `"nema/jsx-dev-runtime"`,
   and run on a big 8 MB host stack — so the actual app-load path on a constrained
   stack was never exercised.

**The real fix** (coordinate the stack budget on every platform + graceful failure):

- **The WASM thread stack is too small.** Emscripten's `DEFAULT_PTHREAD_STACK_SIZE`
  is `0` → falls back to `STACK_SIZE` = **64 KB**, but the JS app worker needs a few
  hundred KB. Fixed by `-sDEFAULT_PTHREAD_STACK_SIZE=1048576 -sSTACK_SIZE=1048576`
  in `targets/wasm`, giving every thread a 1 MB stack. (`thread_host.cpp` stays on
  `std::thread`; an earlier attempt to hand-roll `pthread` + `pthread_attr_setstacksize`
  regressed to a blank/frozen app because Emscripten's pooled-worker model didn't
  start the custom-stack thread — the link flag is the correct mechanism.)
- `JsApp::stackBytes()` is platform-aware: **512 KB** on host/WASM, **256 KB** on
  ESP. `JsApp::onStart` sets the QuickJS guard to **3/4 of the stack on ALL
  platforms** (`setMaxStackSize`), so a too-deep / runaway script throws a **clean,
  catchable** error well before the real stack corrupts — the error card renders,
  the OS keeps running. The ESP-only hardcoded guard in `js_engine.cpp` was removed.
- **ESP app-thread stack moved to PSRAM.** 128 KB didn't fit internal RAM (task
  creation failed → app silently returned to the list). `thread_esp32.cpp` now
  routes large stacks (≥96 KB) to `xTaskCreatePinnedToCoreWithCaps(MALLOC_CAP_SPIRAM)`
  (board has `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y`), so the 256 KB JS stack
  lives in PSRAM and internal RAM is untouched. Such a task is deleted by `join()`
  via `vTaskDeleteWithCaps` after the task parks itself (`vTaskSuspend`), per the
  IDF requirement that WithCaps tasks not self-delete.
- `JsEngine::settleEval()` now inspects the module-evaluation **promise**: an ES
  module that throws at top level evaluates to a *rejected promise*, which the old
  code never checked — so a broken app looked "loaded". Now it surfaces a real
  error (no silent blank).
- `thread_esp32.cpp` fails loudly (signals `join()`, clears `running()`) if the
  task can't be created, instead of leaving the owner waiting forever.
- **`ViewDispatcher::redrawPending_` made atomic.** It was a plain `bool` written by
  `requestRedraw()` (the app thread, via `AppHost::present`) and read by the GUI
  loop's `takeRedraw()` — a cross-thread data race. On WASM (Web Workers +
  SharedArrayBuffer) the compiler could hoist the GUI loop's read into a register
  and never observe the app thread's write, so a freshly-loaded app stayed **blank
  until some GUI-side event** (clock tick, keypress, restart) flipped the flag —
  the intermittent "blank, works after restart" on the simulator. Now
  `store(release)` / `exchange(acquire)`.

**Verified**:
- **Simulator (WASM)**: confirmed working by the user — JS apps now load
  (`compile → eval → jobs → loaded`) and render. Builds clean (`bun run build:wasm`).
- **Host**: 8/8 tests pass, including a new `js_graceful_test` (deep parse, runaway
  recursion, top-level throw → clean errors, then a real app still loads) and
  `js_dualimport_test` (every built-in app loads).
- **ESP (skyrizz-e32)**: firmware builds clean (2.2 MB, 58% partition free).

**Still needs on-device validation**: flash skyrizz-e32 and confirm Sys Info /
Counter now (1) load — logs should show `compile → eval → jobs → loaded` like the
simulator, not just `launch app=…` — and (2) exit/re-enter cleanly several times
(exercises the PSRAM WithCaps task teardown: park-then-`vTaskDeleteWithCaps`).

---

## Notes

- All fixes build cleanly (`cmake --build build-host`) and host tests pass (`ctest`).
- Fix 4's scaled fallback (per-pixel blit) is slower than the direct `flushBuffer` path. For scale > 1.0 fullscreen apps, each frame does up to 150×100 = 15,000 `drawPixel` calls instead of one DMA push. This is acceptable for now; a future optimisation could scale the app buffer up to physical dimensions server-side.
- The intermittent freeze triggered by up/down keys (on any app, any scale) was not reproduced deterministically this session. It may be partially addressed by Fix 3 (modality reset prevented DPM key from landing on the wrong handler), but needs further investigation.
