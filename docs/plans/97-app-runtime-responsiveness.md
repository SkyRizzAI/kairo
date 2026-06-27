# Plan 97 — App Runtime Responsiveness, Throughput & RAM Efficiency

**Status:** analysis (not started) · **Owner:** TBD · **Created:** 2026-06-27

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

- [ ] **P0 — Collapse the app render handoff to same-frame.** After the GUI loop
      sends input to the app mailbox, give the app thread a brief window to produce
      its frame *before* this GUI frame renders (e.g. GUI waits on a per-app
      "frame-ready" signal with a small bounded timeout, ~4–8 ms), so a fast app
      responds in the same frame like a system screen does. Removes the guaranteed
      +1 frame for the common case. *(Core 0 jitter remains; see P0b.)*
- [ ] **P0b — Raise app-thread priority / core placement.** Evaluate moving the app
      thread off the WiFi/BLE-shared core or bumping its priority so radio bursts
      don't stall app input handling. Measure against WiFi/BLE throughput first
      (must not regress radio).
- [ ] **P1 — Event-driven GUI loop.** Replace the fixed `sleepMs(budget)` with a
      blocking wake on input-posted / redraw-requested / nearest-animation-deadline
      (Flipper pattern). `InputService` already owns a `cv`; use `receive(timeout)`
      instead of per-frame `tryReceive`. Cuts 0–33 ms input + 0–33 ms render and
      drops idle CPU (battery win).
- [ ] **P2 — Gesture latency (baseline, affects both paths).** Either drop `doubleMs`
      280→180 ms (1-line mitigation) or make the primary confirm fire on release.
      *Note: user reports Settings OK feels fine, so this is secondary — do after
      P0/P1.*

### 🎯 RAM efficiency (cooperate with PSRAM, don't disturb radios)

- [ ] **P3 — Pack app framebuffer to 1bpp.** `drawBuf_`/`readyBuf_` store mono
      content at **1 byte/pixel** (`app_host.cpp:79-89,183-185`): 320×240×2 ≈ 150 KB
      PSRAM/app. Packing to 1bpp ≈ 19 KB (8× saving). Already in PSRAM (internal RAM
      untouched), but lowers pressure on PSRAM shared with camera/audio. Trade-off:
      unpack bits on the non-fullscreen blit (cheap).
- [ ] **P5 — Deferred app-exit cleanup.** Move `thread_.join()` out of GUI
      `tick()` (`app_host.cpp:197-203`) onto `TaskRunner`/a workqueue (AkiraOS
      pattern) so app exit never stalls a GUI frame.

### 🎯 Throughput / bandwidth

- [ ] **P4 — Dirty-rect end-to-end.** We already compute `getDirtyBounds`
      (`view_dispatcher.cpp:144`) but only use it to clip the canvas. Extend it to
      narrow the present() memcpy **and** the `flushBuffer`/SPI-DMA to the dirty
      region (AkiraOS/LVGL pattern). Cuts per-frame memcpy + SPI traffic, raising
      throughput headroom.

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

- Measure actual app input→pixel latency on hardware (instrument `mailbox_.send`
  timestamp vs `draw()` blit) to quantify the +1 frame and core-0 jitter separately.
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
