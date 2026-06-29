# 0019 — Event-driven GUI loop via a cross-thread Waker

**Status:** accepted · **Date:** 2026-06-29 · **Plan:** [97](../plans/97-app-runtime-responsiveness.md)

## Context

The GUI runs on its own thread (`GuiService::loop`, core 1) and historically paced
itself at a fixed ~30 fps: do a frame, then `Thread::sleepMs(TARGET_FRAME_MS - elapsed)`.
Input was sampled once per frame via `InputService::next()` (a non-blocking poll).

This produced a UX gap that users felt specifically **inside custom apps** (not in
system screens like Settings):

- **System screens** dispatch input *synchronously on the GUI thread*
  (`ViewDispatcher::handleAction` → `screen->onAction`), mutate state, and render in
  the **same frame**. Snappy.
- **Apps** run on a *separate thread*. The GUI thread only drops the input into the
  app's mailbox (`AppHost::onCode`), renders the **old** frame, then sleeps. The app
  processes asynchronously and calls `present()` → `requestRedraw()`. Because the GUI
  thread was asleep, the new frame only appeared on the **next** poll — a guaranteed
  **+1 frame (up to 33 ms)** on top of the input-poll latency (another up to 33 ms).

So the same action felt instant in Settings but laggy in an app.

## Decision

Make the GUI loop **event-driven**: introduce a one-consumer `Waker`
(`core/include/nema/waker.h`, a `std::mutex` + `std::condition_variable` + a
coalescing flag) and replace the loop's blind `sleepMs(budget)` with
`rt_.guiWaker().wait(budget)`. `wait()` returns when the budget elapses **or** when a
producer calls `signal()`, and consumes the flag so a signal racing the wait is not
lost.

**Only cross-thread producers signal the waker:**

- `InputService::post* ` — input arriving mid-frame wakes the loop now (no poll wait).
- `AppHost::present` — an app finishing a frame wakes the loop now (no +1-frame wait).
- `AppHost` terminal output (same app-thread path).

`Runtime` owns the `Waker` (value member) and wires it into `InputService` at
`initCore()`; `AppHost` reaches it via `rt_.guiWaker()`.

The wait stays **capped at `TARGET_FRAME_MS`**, so the 30 fps ceiling and the
DPM/animation/status-bar cadences are unchanged — this is purely a latency cut on the
two edges, not a re-architecture of frame timing.

## Why only cross-thread producers signal (the subtle part)

`ViewDispatcher::requestRedraw()` is deliberately **NOT** a waker signal, even though
`AppHost::present` calls it. `requestRedraw()` is also called every frame *on the GUI
thread itself* by the animation manager, status refresh, and same-thread navigation.
If those signalled the waker, the flag would be set during the frame body, `wait()`
would return immediately every iteration, and the loop would become a **100% CPU
busy-spin** that ignores frame pacing. GUI-thread-internal redraws don't need waking —
they already render in the current frame. Therefore `present()` signals the waker
*explicitly* (in addition to `requestRedraw()`), and `requestRedraw()` stays
signal-free.

## Consequences

- **+** Input→pixel latency in apps drops from "input-poll (≤33 ms) + 1 frame (≤33 ms)"
  to roughly "app processing + scheduling" — apps now feel like system screens.
- **+** Input latency on *all* screens loses the up-to-33 ms poll wait.
- **+** Foundation for an idle-power win later (the wait could be extended past
  `TARGET_FRAME_MS` when nothing is animating — deferred, since DPM/status still need
  periodic ticks).
- **−** A custom app still shows one stale frame for the few ms between input dispatch
  and the app's `present()` (the app is a different thread that must be scheduled). The
  new frame now follows ~immediately, so it is imperceptible; eliminating it entirely
  would need a mid-frame wait-for-app (rejected as higher risk).
- **−** New invariant to respect: **never signal the waker from GUI-thread code.** A
  future contributor adding a signal to `requestRedraw()` would reintroduce the
  busy-spin. Documented in `waker.h` and here.
- App-thread scheduling jitter on the WiFi/BLE-shared core 0 remains (tracked as P0b
  in Plan 97).

Portable across ESP32 (FreeRTOS), host, and the WASM simulator (emscripten pthreads /
web workers) — the same `std` primitives already power `MessageQueue` and `Logger`.
