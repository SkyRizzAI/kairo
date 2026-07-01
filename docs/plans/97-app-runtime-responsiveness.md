# Plan 97 — App Runtime Responsiveness, Throughput & RAM Efficiency

**Status:** in progress (P0+P1 shipped) · **Owner:** TBD · **Created:** 2026-06-27

> Goal of this plan: make custom apps feel **instant** (no perceived delay on
> action), raise render/IPC throughput, and keep RAM/PSRAM usage lean so the app
> runtime never starves the WiFi/BLE controllers. Driven by a UX report: *actions
> inside custom apps feel laggy / not instant, while system screens (Settings)
> feel fine.*

This plan is the output of a deep analysis of our runtime vs two reference
firmwares: **Flipper Zero Momentum** (Furi/FreeRTOS, event-driven GUI) and
**AkiraOS** (Zephyr + WAMR, LVGL dirty-rect rendering).

---

## 1. Root cause — why custom apps feel laggier than system screens

Both paths share the same input front-end (button → gesture engine → InputService
→ GUI loop), so the **gesture double-tap window** (`doubleMs=280ms`,
`core/include/nema/input/gesture.h:54`) is **NOT** the differentiator — it affects
Settings and apps equally. The differentiator is the **dispatch path**:

### System screen (Settings) — synchronous, same-frame

`GuiService::loop` (`core/src/services/gui_service.cpp:136`) drains input at the
**top** of a frame and renders at the **bottom** of the **same** frame:

```
handleAction(a) → ViewDispatcher::handleAction  (view_dispatcher.cpp:150)
                → screen->onAction(a)            // SYNCHRONOUS, on GUI thread
                → screen mutates state + requestRedraw()   // same thread
... later same frame ...
render: vd.takeRedraw() == true → renders THIS frame
```

**Latency ≈ input-poll wait + render ≈ 0–33 ms.** Snappy.

### Custom app — cross-thread, +1 frame, +scheduling jitter

The active "screen" is an `AppHost` (an `IScreen` wrapper). Its `onAction`/`onCode`
do **not** run app logic and do **not** request a redraw — they only hand the event
to another thread:

```
handleAction(a) → AppHost::onAction  (app_host.cpp:123)  // just stores pendingAction
handleCode(c)   → AppHost::onCode    (app_host.cpp:127)  // mailbox_.send(ie)  → app thread
        (no requestRedraw → this GUI frame renders OLD content)

App thread (core 0, prio 5, shared with WiFi/BLE):
  waitInput() wakes (cv) → app handler runs → draws to drawBuf_
  → present()  (app_host.cpp:238): memcpy 75 KB drawBuf_→readyBuf_, frameSeq++,
    requestRedraw()
        ↓
GUI frame N+1 (up to 33 ms later): takeRedraw() == true → blits readyBuf_
```

**Extra cost vs a system screen:**

1. **Guaranteed +1 frame** (≈16–33 ms): the producer/consumer thread split means the
   redraw is requested *after* the current GUI frame already rendered.
2. **App-thread scheduling jitter:** the app thread is pinned to **core 0**, the same
   core as the WiFi/BLE stacks, at **priority 5** (below radio tasks). A radio burst
   delays the app handler → *variable* lag, exactly the "kadang tidak responsif"
   symptom.
3. **present() full-frame memcpy** (~75 KB) under `frameMtx_` every time the app draws.

Code already carries scar tissue from this race — the "safety net" in
`AppHost::tick` (`app_host.cpp:206-209`) force-requests a redraw because app frames
sometimes arrive late.

**Conclusion:** the felt delay in apps is the **app dispatch/render handoff**, not
the gesture window. Fixing the gesture window only shaves a constant baseline that
Settings shares; it will not close the Settings-vs-app gap.

---

## 2. Latency budget (skyrizz-e32, "press → app reacts")

| Stage | Mechanism | System screen | Custom app |
|---|---|---|---|
| Button edge | `Xl9535::tick` IRQ-flag + 15 ms poll, ticked ~5 ms on main loop (`esp32_platform.cpp:416` `vTaskDelay(5)`) | ~5 ms | ~5 ms |
| Gesture (OK only) | `doubleMs=280` deferral, double-mode middle btn (`e32_key_map.cpp:13`) | +280 ms | +280 ms |
| Input drain | `InputService::next` polled 1×/frame (`input_service.h:42`) | 0–33 ms | 0–33 ms |
| Dispatch | sync `onAction` | **same frame** | cross-thread mailbox + **core-0 jitter** |
| Process+present | — | inline | app handler + 75 KB memcpy |
| Render | `takeRedraw` | **same frame** | **+1 frame (0–33 ms)** |
| **Net (non-OK)** | | **~16–35 ms** | **~50–100 ms + radio jitter** |

---

## 3. Comparison with reference firmwares

| Dimension | Palanu (ours) | Flipper Momentum | AkiraOS |
|---|---|---|---|
| App = own thread | ✅ `AppHost` | ✅ FreeRTOS task | ✅ Zephyr thread |
| **GUI loop** | ❌ polling 30 fps (`gui_service.cpp:140`) | ✅ event-driven `furi_thread_flags_wait` | LVGL 1 ms tick + flush on-demand |
| **Input wake** | poll `tryReceive`/frame | ✅ ISR→flag→PubSub, GUI wakes instantly | GPIO IRQ→workq |
| App input dispatch | cross-thread mailbox, async | event queue on app thread | per-app thread |
| Confirm/select | OK gated 280 ms (double-mode) | Short fires **on release** | LVGL gesture, no global hold |
| **Render** | full-frame blit + flushBuffer | layer composite + `canvas_commit` | ✅ **dirty-rect partial** (10% buf) + DMA |
| Stack policy | app→PSRAM, GUI/btn→internal | static pool | ✅ stack **always internal SRAM**, lazy alloc |
| Big buffers | PSRAM-prefer (`allocBuf`) | CCRAM pool + heap | ✅ PSRAM-prefer allocator |
| App-exit cleanup | `join()` in GUI `tick()` (`app_host.cpp:200`) | thread self-suspend | ✅ **deferred via workqueue** (O(1) exit) |
| Lock granularity | per-AppHost frame mutex | single GUI mutex | ✅ per-slot mutex |

**Patterns worth adopting:** (a) Flipper's event-driven GUI wake; (b) AkiraOS's
dirty-rect partial flush + deferred exit cleanup; (c) Flipper's fire-on-release for
the primary confirm.

What we already do right (keep): per-app threads, `TaskRunner` offload
(`task_runner.h`), unbounded `AsyncEventPoster` for radio events, app buffers in
PSRAM so internal RAM stays free for WiFi/BLE (`app_host.cpp:79` documents the
internal-RAM-exhaustion reboot we already fixed), `cv.wait()` (not `wait_for`) to
avoid watchdog-starving busy-spin (`message_queue.h:42`).

---

## 4. Proposed work (prioritized) — maps to the three goals

### 🎯 Responsiveness (kill the app-vs-Settings gap)

- [x] **P0+P1 — Event-driven GUI loop (shipped 2026-06-29).** Added a `Waker`
      primitive (`core/include/nema/waker.h`) and replaced the GUI loop's blind
      `sleepMs(budget)` with `rt_.guiWaker().wait(budget)`
      (`gui_service.cpp`). The loop now wakes **early** when a cross-thread
      producer signals: input posted (`InputService::post*`) or an app finished a
      frame (`AppHost::present`). This removes the up-to-33 ms poll latency on the
      input edge **and** the app-present edge (the old guaranteed +1 frame): a
      fast app's frame now reaches the panel ~immediately after `present()` instead
      of waiting out the budget. The 30 fps ceiling and DPM/animation/status cadence
      are unchanged (wait is still capped at `TARGET_FRAME_MS`). **Critical design
      choice:** only cross-thread producers signal — GUI-thread-internal redraws
      (animation/status/navigation) do NOT, because they already render in the
      current frame and signalling per-frame would busy-spin the loop. See ADR 0019.
      Validated: host build + 27/27 tests + WASM simulator build all green.
- [~] **P0b — App-thread core/priority now TUNABLE (shipped 2026-06-29; default
      unchanged).** `AppHost::onResume` reads `app/thread_prio` (default 5) and
      `app/thread_core` (default 0) from config and passes them to the thread start.
      Defaults reproduce the original {prio 5, core 0}, so behavior is unchanged
      until someone opts in. This makes the "move app off the WiFi/BLE core 0"
      experiment a config flip (`app/thread_core=1` → app shares the GUI core 1,
      freeing core 0 for radio) instead of a code change — so it can be A/B tested
      on hardware (measure radio throughput + the `AppLatency in→present` jitter)
      before adopting any non-default value. **Decision still pending hardware
      measurement** — must not regress WiFi/BLE.
- [x] **P2 — Gesture latency (shipped 2026-06-29).** `doubleMs` default 280→220
      (`gesture.h`). A single tap on a double-mode button (E32 OK→Activate) fires
      ~60 ms sooner; the double-tap=Back mapping is unchanged (window still 220 ms,
      comfortable for a deliberate double-tap). Non-breaking, 1-line. Validated:
      host + 27/27 tests. *(Did not pursue fire-on-release — that changes the Back
      mapping and is more invasive.)*

### 🎯 RAM efficiency (cooperate with PSRAM, don't disturb radios)

- [x] **P3a — Lazy framebuffer allocation (shipped 2026-06-29).** The two
      full-screen buffers (~150 KB PSRAM) were allocated in `onResume()` for *every*
      app at launch — wasted on terminal/CLI apps (BadUSB, shells, compute-only
      WASM/JS) that never draw. Now deferred to a new `AppHost::ensureCanvas()`
      called on first `canvas()`/`present()` (`app_host.cpp`). Non-GUI apps allocate
      0 framebuffer RAM; GUI apps unchanged. Validated: host + 27/27 tests + WASM.
- [~] **P3b (app buffers) — Pack the per-app framebuffer to 1bpp (code done
      2026-06-29; awaiting hardware visual-check).** New shared helper
      `core/include/nema/hal/mono1.h` defines the packed layout (contiguous bit idx
      `y*w+x`, MSB-first) — the SAME convention `LcdDriver::framebuf_` already used,
      so this unifies the format rather than inventing one. `BufferDisplay` now packs
      (drawPixel/fillRect/clear/invertRect via `mono1`), `AppHost` allocates/blits
      packed (`byteSize`, `mono1::get`), and skyrizz `LcdDriver::flushBuffer` +
      `prevBuf_` read packed bits. **dev-board needs no change** (async-display →
      slow-blit path). Saving ≈ **130 KB PSRAM per GUI app** (150 KB → ~19 KB).
      Forward-compat: a future RGB UI mode is an additive `PixelFormat::Rgb565` path;
      Mono1 stays the default (see `mono1.h`). **Validated:** host + 27/27 tests +
      WASM simulator (slow-blit `mono1::get`). **Not yet:** the skyrizz `lcd_driver`
      fast-path could not be ESP32-compiled because a parallel `http_client` WIP
      currently breaks the ESP32 build upstream — once that lands, build + flash and
      visually confirm app rendering + colour themes (fg/bg palette) are correct.
- [~] **P3b (system buffers) — Pack async_display + eink (dev-board) to 1bpp
      (code done 2026-06-29; awaiting dev-board visual-check).** `AsyncDisplayDriver`
      (core) and `EinkDisplay` (dev-board) now store/diff/flush 1-bit packed via
      `mono1`: the 3 async buffers drop ~46 KB→~5.8 KB each (~120 KB saved) and the
      eink buf/prev drop likewise. **Zero impact on skyrizz or the simulator** —
      `AsyncDisplayDriver` is registered only by dev-board (skyrizz uses `lcd_`
      directly, whose `framebuf_` was already 1-bit). Validated: host build compiles
      `async_display`; `eink_display` is ESP32-only (matched the exact `mono1`
      convention; dev-board flash + visual-check pending, low risk as e-ink is
      natively 1-bit B&W).
- [ ] **P5 — Deferred app-exit cleanup.** Move `thread_.join()` out of GUI
      `tick()` (`app_host.cpp:197-203`) onto `TaskRunner`/a workqueue (AkiraOS
      pattern) so app exit never stalls a GUI frame.

### 🎯 Throughput / bandwidth

- [ ] **P4 — Dirty-rect / partial redraw.** Deep research done → see
      **[Plan 98](98-dirty-rect-rendering.md)**. Key findings: the dirty-rect
      machinery already exists but is dormant (full `requestRedraw()` = 118 sites,
      partial = ~2); compositing is *not* the blocker (clip recomposites correctly);
      skyrizz SPI is already row-diffed by the driver, so P4's win is CPU-bound and
      concentrated in component screens + the slow-blit path. Recommended route is
      **automatic tree-diff** (our UI rebuilds a UiNode tree each frame) to avoid the
      manual-annotation ghosting risk. **Gated on measuring draw-ms via the FPS
      overlay first** (Plan 98 §5).

### Effort / risk

| Item | Effort | Risk | Impact |
|---|---|---|---|
| P0 same-frame handoff | medium | medium (timing) | removes +1 frame in apps |
| P0b core/prio | small-med | med (radio regression) | removes app jitter |
| P1 event-driven GUI | medium | medium | −0..66 ms all input + power |
| P2 gesture | 1 line / medium | low / UX | −100..280 ms baseline |
| P3 1bpp buffer | medium | low | −130 KB PSRAM/app |
| P4 dirty-rect flush | medium-large | medium | bandwidth/throughput |
| P5 deferred exit | small | low | no GUI stall on exit |

**Suggested order:** P0 → P1 → P0b → P3 → P4 → (P2, P5 opportunistically).

---

## 5. Open questions / before implementing

- [x] **Measure app input→pixel latency (shipped 2026-06-29).** Opt-in
  instrumentation in `AppHost` (config `aether/applatency=1`, default off). Stamps
  the input as it enters the app mailbox (GUI thread), then logs two segments:
  `in→present` (app reaction = scheduling + draw, exposes the core-0 jitter that
  P0b targets) and `in→pixel` + `present→pixel` (the GUI handoff that P0+P1
  collapsed to ~0). Tag `AppLatency`, info level. Enable: set config `aether`
  `applatency` to `1`, then watch the serial/sim log while interacting with an app.
- P0b: confirm WiFi/BLE throughput does not regress if the app thread is re-pinned
  or re-prioritized.
- P3: verify all display backends are truly mono (1-bit) on every board before
  packing; color boards would need a different packing.

---

## 6. Key files (reference)

| Area | File |
|---|---|
| GUI render loop | `firmware/core/src/services/gui_service.cpp:136` |
| App host (per-app thread, double-buffer, present) | `firmware/core/src/app/app_host.cpp` |
| System-screen dispatch | `firmware/core/src/ui/view_dispatcher.cpp:150` |
| Input funnel | `firmware/core/include/nema/services/input_service.h` |
| Gesture timing | `firmware/core/include/nema/input/gesture.h:51-54` |
| E32 keymap (double-mode OK) | `firmware/boards/skyrizz-e32/src/e32_key_map.cpp:13` |
| Button poll source | `firmware/boards/skyrizz-e32/src/xl9535.cpp:64` |
| Thread primitive | `firmware/core/include/nema/thread.h` |
| Task offload | `firmware/core/include/nema/task_runner.h` |
| Cross-thread events | `firmware/core/include/nema/event/async_event_poster.h` |
