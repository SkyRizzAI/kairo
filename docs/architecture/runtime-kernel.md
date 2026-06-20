# Runtime & Kernel

> Core runtime and the "Nema" kernel: the hardware-free engine that boots the device,
> owns every long-lived singleton, and provides the threading primitives that keep the
> UI from ever blocking.

## Purpose

The core runtime boots a device through a strict phase machine, owns all long-lived
singletons (logger, event bus, service registry, capability model), and provides
portable threading so the UI thread never blocks. Everything below the
`IPlatform`/`IBoard` seam is platform-free: the same `firmware/core/` runs on ESP32
(FreeRTOS) and on the WASM simulator (`std::thread`), selected only by which
`thread_*.cpp` is compiled. `Runtime` (`firmware/core/include/nema/runtime.h`) is the
single composition root.

## Boot flow

Entry is the target's `main.cpp` (e.g. `firmware/targets/skyrizz-e32/main/main.cpp`).
Implementation in `firmware/core/src/runtime.cpp`:

1. `Runtime::create()` — empty Runtime, phase `None`.
2. `loadPlatform(p)` → phase `PlatformLoaded`.
3. `loadBoard(b)` → phase `BoardLoaded`.
4. `initCore()` — constructs kernel singletons in order: `MemorySink(1024)` + `Logger`
   (bound to `platform_->clock()`); `ConsoleSink` only when `outputMode()==Human`; then
   `EventBus`, `ServiceContainer`, `HardwareRegistry`, `CapabilityRegistry` (wired to the
   bus so liveness changes emit events), `CliSessionManager`, `SystemInfo`, `AppRegistry`,
   `AppHostManager`, `ViewDispatcher`. Phase → `CoreReady`.
5. `registerServices()` — the heart of bring-up:
   - `platform_->registerDrivers(*this)` — platform installs its drivers/services.
   - `board_->describeHardware(*this)` — board declares capabilities + hardware entries
     and installs its keymap (the pin map / capability source of truth).
   - `platform_->postRegister(*this)` — platform decorates board drivers (e.g. wraps the
     display with a remote screen-tap) **before** Canvas binds.
   - Constructs `ServiceManager`; builds the `Canvas` if the `Display` capability is
     present (logical scale from config `display/scale` or driver `dpi()`).
   - Resource-liveness bridge (Plan 42): seeds `NetWifi`/`BtBle` as `Absent`, mirrors
     network/BT connect events into `CapabilityRegistry::setState`.
   - Crash-recovery init (Plan 64); conditionally adopts `DummyBatteryDriver` (if Display)
     and `NtpService` (if NetWifi). Publishes `events::SystemBoot`. Phase → `ServicesRegistered`.
6. *(optional)* targets install app-services, e.g. `rt.apps().installService(...)`.
7. `start()` — `serviceManager_->startAll()` (registration order), `taskRunner_.start()`,
   then constructs+starts `GuiService` (spawns the UI thread). Phase → `Running`; publishes
   `events::SystemReady`.
8. Targets install apps and `rt.view().push(homeScreen)`, then enter the loop.
9. Loop: Arduino `loop()` calls `rt.step()`; host/WASM `rt.run()` loops `step()` until shutdown.

`step()` per frame: (1) `asyncPoster_.flush(*eventBus_)` drains cross-thread events FIRST
so subscribers see them this frame; (2) `serviceManager_->tickAll(now)`; (3)
`platform_->idle()`. **Rendering is not here** — it lives on the GUI thread.

## Key abstractions

- **Runtime** (`include/nema/runtime.h` / `src/runtime.cpp`) — composition root and façade.
  Owns all singletons, drives the boot phase machine and the `step()`/`run()` loop, exposes
  accessors (`log()`, `events()`, `container()`, `capabilities()`, `tasks()`, `view()`,
  `config()`…), power requests (→ `platform_->power()`), and `adoptService`/`dropService`.
- **ServiceContainer** (`include/nema/service/service_container.h`, template-only) —
  type-indexed DI registry. `registerService<T>` stores by `typeid` and, if `T : IService`,
  also enrolls it in an insertion-ordered lifecycle list. `registerAs<I,T>` aliases under an
  interface. `resolve<T>()` → nullptr if absent; `require<T>()` asserts.
- **ServiceManager** (`service_manager.*`) — service state machine. `startAll()` in container
  order; `stopAll()` reversed. A throwing `start()` → `Failed` (not a crash, not retried).
  `tickAll(now)` ticks only `Running` services; transitions publish lifecycle events.
  `IService` = `name()/start()/stop()/tick(nowMs)`.
- **Nema Thread** (`include/nema/thread.h`; `src/nema/thread_esp32.cpp` & `thread_host.cpp`)
  — portable thread with `ThreadConfig{name, stackBytes, priority, core}`. **Cooperative stop
  only**: `requestStop()` sets an atomic; entry bodies must loop `while(!shouldStop())`. ESP32
  uses `xTaskCreate`/PinnedToCore; stacks ≥96 KB go to PSRAM (`...WithCaps`) and are freed by
  `join()` (a task can't free its own PSRAM stack). Host uses `std::thread`.
- **MessageQueue<T>** (`include/nema/message_queue.h`, header-only) — thread-safe `deque` +
  mutex + condition_variable. `capacity==0` = unbounded (never drops); bounded `send` returns
  false when full. **Not ISR-safe** (input is polled from a thread, not signalled from an ISR).
- **TaskRunner** (`task_runner.*`) — the "never freeze" primitive. One serial worker thread;
  `submit(Job, Done)` runs `Job` on the worker (may block), then queues `Done` back to the UI
  thread via `drainCompletions()` (called from the GUI loop, so completions touch UI state
  safely). Worker: `{"nema_worker", 8192, prio 4, core 0}`.
- **Capability / Resource model** (`system/capability_registry.*`, Plan 42 — two axes).
  Axis 1 = static inventory (`add/has/list`, append-only). Axis 2 = dynamic liveness
  (`setState/stateOf/available` over `ResourceState{Absent,Available,Fault}`); `setState`
  publishes `ResourceChanged` and only the **owning service** may call it. A static capability
  that never reports liveness reads as `Available`. **HardwareRegistry** is a parallel
  inventory of concrete `HardwareEntry{id, DriverKind, detail}`.
- **EventBus** (`event/event_bus.*`) — single-threaded synchronous pub/sub; `subscribe(name)`
  (`"*"` = wildcard); `publish` snapshots the sub list so handlers may (un)subscribe
  re-entrantly. **AsyncEventPoster** (`event/async_event_poster.*`) is the cross-thread front
  door: background threads `post()` into an unbounded queue, the main thread drains via
  `flush(bus)` at the top of `step()`. Never call `EventBus::publish` off the main thread.
- **Logger + sinks** (`log/logger.*`) — mutex-guarded `log()` callable from any thread; fans a
  `LogEntry` out to every `ILogSink`. **ConsoleSink** → stdout (`[HH:MM:SS] [LEVEL] [Component]
  msg key=value`); **MemorySink** → ring buffer (capacity 1024) read by the on-device Logs
  screen via `Runtime::logForEach`/`logCount`.
- **ConfigStore** (`config/config_store.h`, interface in core; impls platform-side) —
  namespaced KV (ns/key ≤15 chars per NVS limit), `getString/getInt`, `setString/setInt`
  (commit immediately). ESP32 → NVS; simulator → in-memory.
- **Process model** (`proc/`, Plan 54) — `ProcessContext` is the runtime-agnostic kernel
  surface a process sees (args/cwd/env, stdio, `requestExit/shouldExit`, `runtime()`).
  `ProcessHost` spawns the app on its own Nema thread (`{"nema_app", app.stackBytes(), prio 5,
  core 0}`) and calls `app_.runProcess(*this)`. `ProcessManager` is a non-owning table for `ps`.
- **Crypto** (`crypto/sha256.*`) — portable `hexSha256` + `randomHexSalt` (XorShift32, NOT
  cryptographically secure — rainbow-table defence only) for ProfileService password hashing.

## Threading model

| Thread | Created by | Config | Runs |
|---|---|---|---|
| Main / loop | platform entry (`loop()`/`run()`) | core 1 (Arduino) | `step()`: flush async events → `tickAll` → `idle()`. No rendering. |
| GUI / UI | `GuiService::start()` | `{"nema_gui", 8192, prio 6, core 1}` | Sole owner of Canvas + ViewDispatcher: input → screens, status bar, anim tick, `TaskRunner::drainCompletions()`, render+flush. |
| Worker | `TaskRunner::start()` | `{"nema_worker", 8192, prio 4, core 0}` | Serial FIFO blocking jobs; hands `Done` back to UI thread. |
| App / process | `ProcessHost::start()` / `AppHost` | `{"nema_app", app.stackBytes(), prio 5, core 0}` | `IApp::run()`/`runProcess()`; cooperative exit. |
| Driver background | platform drivers (WiFi/BLE/NTP/HTTP) | platform-defined | Post into `AsyncEventPoster`; never touch EventBus/UI directly. |

Cross-thread rules: any thread → main via `AsyncEventPoster::post`; worker → UI via TaskRunner
completions; UI render is single-owner so no UI mutex is needed (provided apps never mutate UI
from their tick). Shutdown order in `run()`: stop GUI thread first, join worker, then
`stopAll()` (reverse order).

## Conventions & gotchas

- **Phase asserts are load-bearing** — each boot method asserts the prior phase; `start()`
  requires exactly `ServicesRegistered`.
- **`postRegister` must do display decoration** — Canvas binds to the display driver right
  after, inside `registerServices`.
- **Service order = container insertion order**; stop is reversed. Register in dependency order.
- **Never `EventBus::publish` off the main thread** — use `AsyncEventPoster::post` (unbounded
  by design; events must not be lost).
- **Capability liveness is owner-only**; a static cap with no liveness reads as `Available`.
  Check capabilities, never board name / `#ifdef`.
- **Cooperative threading only** — no forced kill; entry loops must check `shouldStop()`.
  ESP32 PSRAM-stack tasks (≥96 KB) must be freed from another context (`join()` handles it).
- **Logging must go through `rt.log()`** — MemorySink is read by `dynamic_cast`; swapping the
  sink type would silently break the Logs screen.
- **Restart/bootloader exit with code 75** on host/sim so the supervisor relaunches.

## Key files

| File | Description |
|---|---|
| `firmware/core/include/nema/runtime.h` + `src/runtime.cpp` | Runtime façade, boot phase machine, `step()`/`run()`, shutdown. |
| `firmware/core/include/nema/thread.h` + `src/nema/thread_{esp32,host}.cpp` | Portable Thread (cooperative stop, PSRAM stacks). |
| `firmware/core/include/nema/message_queue.h` | Thread-safe bounded/unbounded queue. |
| `firmware/core/include/nema/task_runner.h` + `src/nema/task_runner.cpp` | Worker offload, UI-safe completions. |
| `firmware/core/include/nema/service/service_container.h` | Type-indexed DI + lifecycle list. |
| `firmware/core/include/nema/service/service_manager.h` + `.cpp` | Service state machine. |
| `firmware/core/include/nema/system/capability_registry.h` + `.cpp` | Two-axis capability/resource model. |
| `firmware/core/include/nema/system/hardware_registry.h` + `.cpp` | Concrete hardware inventory. |
| `firmware/core/include/nema/event/event_bus.h` + `.cpp` | Synchronous pub/sub. |
| `firmware/core/include/nema/event/async_event_poster.h` + `.cpp` | Cross-thread event ingress. |
| `firmware/core/include/nema/log/logger.h` + `src/log/{console,memory}_sink.cpp` | Multi-sink logger. |
| `firmware/core/include/nema/config/config_store.h` | Namespaced persistent KV interface. |
| `firmware/core/include/nema/proc/process_{context,host,manager}.h` + `.cpp` | Process model. |
| `firmware/core/include/nema/crypto/sha256.h` + `src/crypto/sha256.cpp` | SHA-256 + salt. |
