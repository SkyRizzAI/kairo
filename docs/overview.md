# Palanu — Project Overview

> **A single-file snapshot of the whole project.** Read this and you should understand what Palanu
> is, how it is built, what works today, how the code is organized, and where it is going — without
> opening any other file.
>
> For the detailed, per-subsystem "how it works" reference, see
> [`architecture/`](architecture/README.md). For current status, see [`STATE.md`](STATE.md).
>
> **Last updated:** 2026-06-13 · **Status:** Full runtime — app model, retained-mode component UI,
> QuickJS custom apps, WiFi/HTTP/BLE/USB, KLP remote link, WASM simulator, and two real ESP32-S3
> boards (e-ink dev board + SkyRizz E32 multimedia badge).

---

## Table of Contents

1. [What is Palanu](#1-what-is-palanu)
2. [Naming](#2-naming)
3. [Current status (verified)](#3-current-status-verified)
4. [Architecture at a glance](#4-architecture-at-a-glance)
5. [Hardware tiers](#5-hardware-tiers)
6. [Repository structure](#6-repository-structure)
7. [Core Runtime](#7-core-runtime)
8. [Core subsystems](#8-core-subsystems)
9. [Nema kernel & threading model](#9-nema-kernel--threading-model)
10. [HAL — driver interfaces](#10-hal--driver-interfaces)
11. [UI Runtime (retained-mode component tree)](#11-ui-runtime-retained-mode-component-tree)
12. [Input abstraction](#12-input-abstraction)
13. [App model](#13-app-model)
14. [JavaScript apps (QuickJS + .kapp)](#14-javascript-apps-quickjs--kapp)
15. [KLP link layer & remote control](#15-klp-link-layer--remote-control)
16. [Platform layer](#16-platform-layer)
17. [Board layer](#17-board-layer)
18. [Target layer](#18-target-layer)
19. [Forge — web simulator + remote/flash tool](#19-forge--web-simulator--remoteflash-tool)
20. [nema-app-sdk](#20-nema-app-sdk)
21. [Build & run](#21-build--run)
22. [Locked architecture decisions](#22-locked-architecture-decisions)
23. [Roadmap & plan status](#23-roadmap--plan-status)
24. [Known gaps](#24-known-gaps)
25. [Glossary](#25-glossary)

---

## 1. What is Palanu

Palanu is a **hardware-agnostic firmware runtime for portable multi-tool devices** in the spirit of
Flipper Zero. It is **one firmware core that runs on many boards**: write an app once and it runs in
the browser simulator and on real hardware, without touching the app per board. Bringing up a new
device means writing **drivers + a pin map**, not a new firmware.

The same portable **C++17 core** runs unchanged across three environments:

- a **web simulator** — the firmware compiled to **WebAssembly**, running fully in the browser (no
  native binary, no server);
- an **ESP32-S3 e-ink dev board** (264×176 mono e-ink); and
- the **SkyRizz E32** multimedia badge (240×320 TFT LCD + touch + camera + mic/speaker).

The goal is not to clone the genre but to give it a foundation good enough to outgrow it: a portable
runtime, a real driver model, a standardized app ecosystem (including sandboxed JavaScript apps), and
simulator-first development.

### Guiding principles

| Principle | Meaning |
|---|---|
| **One core, many boards** | App/UI code never branches on board type — it asks the runtime *"do you have a camera?"* |
| **Drivers, not forks** | A new device is a folder of drivers + a pin map. Core, apps, settings, UI come for free. |
| **Simulate everything** | The whole firmware runs in a web simulator. Build & demo with zero hardware. |
| **Capability driven** | Features light up from what the board declares: `capabilities().has("audio.input")`, never `isEsp32()`. |
| **Resolution independent** | Draw from `canvas.width()/height()`; the same UI fits a 240×320 LCD and a 264×176 e-ink panel. |
| **Never freeze** | Blocking work (WiFi scan, HTTP) runs on worker threads; the UI keeps rendering. |
| **One logger** | All logging goes through `rt.log()`, fanning out to console + an on-device ring buffer. |

---

## 2. Naming

The project has gone through naming churn; here is the canonical map so the codenames don't confuse you.

| Name | What it is |
|---|---|
| **Palanu** | The product — the OS / ecosystem / brand (user-facing). This is the canonical project name. |
| **Nema** (νῆμα, "thread") | The kernel layer — threading primitives + the C++ namespace **`nema::`** that *all* firmware code lives in. ("Palanu runs the Nema kernel.") |
| **Forge** | The web tool: WASM simulator + remote-control + firmware flasher (`packages/forge/`). |
| **KLP → NLP** | The binary remote-control wire protocol (BLE / USB-CDC / virtual cable). Code still spells it **KLP** (`nema::klp`, `klp_codec`); plan 41 renames the *name* to **NLP** (Nema Link Protocol) — the wire format is unchanged. |
| **`nema` (npm)** | The app SDK package authors import from when writing custom `.kapp` apps (`packages/nema-app-sdk/`). |
| **SkyRizz E32** | The flagship multimedia board (ESP32-S3 + TFT + camera + audio). |
| **Kairo** *(retired)* | The project's older name. It still lingers in a few places not yet migrated — the repo directory (`kairo/`) and the `README.md`. Anywhere you see "Kairo," read "Palanu." |

> Bottom line: **product = Palanu**, **kernel / C++ namespace = Nema (`nema::`)**, **link protocol =
> KLP (being renamed NLP)**, **web tool = Forge**. The C++ namespace stays `nema::` (the kernel is
> Nema); only the user-facing brand is Palanu. "Kairo" is the old name on its way out (see plan 41).

---

## 3. Current status (verified)

| Area | Status | Notes |
|---|---|---|
| Core Runtime (boot, logger, event bus, services, introspection, config) | ✅ HW | runs on WASM sim + ESP32 |
| Nema kernel (`Thread`, `MessageQueue`, `TaskRunner`) | ✅ HW | worker threads, no UI freeze |
| App model (`IApp`/`AppHost`/`AppContext`, each app on its own thread) | ✅ HW | replaced the old plugin runtime |
| App registry (`AppRegistry`/`AppManifest`, built-in + custom + service apps) | ✅ HW | install / list / launch |
| Retained-mode component UI (`UiNode` tree, flexbox layout, focus, scroll, momentum) | ✅ | all system screens migrated |
| Input abstraction (`Action` intents, `IKeyMap`, gestures, touch/pointer) | ✅ | board-mapped buttons + FT6336U touch |
| Virtual keyboard + text input (QWERTY, CAPS, 123/sym, password mask) | ✅ | 2D and 1D (3-button) nav |
| WiFi (scan / pick / password / connect, non-blocking) | ✅ build | sim ✓; esp32 driver + NVS |
| HTTP client HAL (esp_http_client + TLS) + Ticker app (BTC/USD) | ✅ build | sim live 200; UI never freezes |
| Bluetooth LE (peripheral, advertise, numeric-comparison pairing, bonds) | ✅ build | `IBleAdapter` + Bluetooth app |
| USB CDC (composite TinyUSB) | ✅ build | raw byte pipe, carries KLP |
| KLP remote link (screen mirror, input, logs, events, CLI, files, OTA, app-install) | ✅ | BLE + USB + WASM cable |
| QuickJS JS engine + `.kapp` custom apps + `nema-app-sdk` | ✅ build | embedded + OTA (volatile) install |
| Camera (`ICamera`, GC2145 DVP) + live viewfinder app | ✅ build | SkyRizz E32 |
| Audio (`IAudioInput`/`IAudioOutput`, ES7243E mic + NS4168 speaker) | ✅ build | Sounds screen meters + test tone |
| Config store (NVS on esp32, RAM on host) + VFS filesystem (LittleFS + MemFS) | ✅ | persistent settings/profile |
| Sleep / lock (`DisplayPowerManager` + lock screen) | ✅ | idle timeouts, configurable |
| User profile (owner / device name / password hash) | ✅ | SHA-256 + salt, NVS-backed |
| WASM simulator in Forge (browser Web Workers, pthreads) | ✅ | KLP over postMessage cable |
| **Dev Board** (ESP32-S3 + e-ink + 6 buttons) | ✅ HW | builds, flashes, runs on device |
| **SkyRizz E32** (ESP32-S3 + TFT + touch + camera + audio) | ✅ build | board + target in tree |

**The "never freeze" pillar is real:** WiFi scan (1–3 s) and HTTP fetch (1–3 s) run on a `TaskRunner`
worker thread while the GUI thread keeps rendering. Apps each run on their own thread and may block
freely; they only ever share a pixel buffer (mutex) and an input queue with the GUI thread.

---

## 4. Architecture at a glance

Palanu is built in four layers; each depends only on the one below it, through abstract interfaces.

```
  targets/     buildable apps           (wasm, dev-board, skyrizz-e32, + bring-up tests)
     │ assembles
  boards/      drivers + pin map        (simulator, dev-board, skyrizz-e32)
     │ provides hardware
  platforms/   SoC / host integration   (esp32, wasm)
     │ implements interfaces
  core/        hardware-agnostic C++17   (Runtime, Nema kernel, HAL, UI, app model, services, JS engine)
```

**The rule:** files under `firmware/core/**` may **never** `#include` anything platform-specific
(no `<Arduino.h>`, no ESP-IDF, no Emscripten, no `nlohmann/json`). Core only knows abstract interfaces
(`IPlatform`, `IBoard`, `IClock`, `IDriver`, `IDisplayDriver`, `IWifiDriver`, `IBluetoothController`,
`ICamera`, `IAudioInput`/`IAudioOutput`, `IHttpClient`, `IFileSystem`, `IConfigStore`, `IKeyMap`,
`ITouchDriver`, `ILinkTransport`, `IService`, `IScreen`, `IApp`). Everything hardware-specific lives
in Platform/Board/Target.

**Why this matters:** every hardware feature is a **subsystem** with the same shape — core provides
the HAL contract, a board provides the driver. Learn the pattern once; add any hardware. Moving to a
new board only requires a new *board layer*; core and the `esp32` platform are reused untouched.

---

## 5. Hardware tiers

| Tier | `IBoard::name()` | What it is | Status |
|---|---|---|---|
| **Simulator** | `simulator` | Virtual board; the firmware compiled to **WASM** runs in the browser (or host tests) with virtual drivers. No hardware. | ✅ running |
| **Dev Board** | `dev-board` | ESP32-S3-WROOM-1-N8R8 (8 MB flash / 8 MB PSRAM) + 2.7″ e-ink 264×176 + 6 buttons via TCA9534 + ATECC608B secure element. Interim e-ink test rig. | ✅ runs on device |
| **SkyRizz E32** | `skyrizz-e32` | ESP32-S3-WROOM-1-N16R8 (16 MB flash / 8 MB PSRAM) + 240×320 TFT LCD + FT6336U touch + GC2145 camera + ES7243E mic + NS4168 speaker + XL9535 expander + sensors (AHT20, light, accel) + RGB LED. The multimedia flagship. | ✅ builds (HW bring-up in progress) |

A future custom PCB ("Palanu Board V1") is referenced in older docs but not yet designed; it would
reuse the `esp32` platform + core with only a new board layer.

---

## 6. Repository structure

A Bun monorepo. `firmware/` is C++; `packages/` is the TypeScript/web tooling.

```
palanu/                         ( repo dir is still "kairo/" — rename pending, plan 41 )
├─ package.json              # bun workspaces + scripts (forge, build/flash:*, test); name: "palanu"
├─ README.md                 # public-facing intro (still says "Kairo" — not yet migrated)
├─ release-please-config.json + .release-please-manifest.json   # automated versioning
│
├─ firmware/                 # ── all C++ ──
│  ├─ CMakeLists.txt         # top-level HOST build (C++17, -Wall -Wextra)
│  ├─ VERSION                # "0.1.0 # x-release-please-version"
│  ├─ cmake/                 # nema_version.cmake + version.h.in (git-hash version header gen)
│  │
│  ├─ core/                  # HARDWARE-AGNOSTIC runtime (dual-build: host + ESP-IDF), ~12k LOC
│  │  ├─ include/nema/
│  │  │  ├─ runtime.h, board.h, platform.h, clock.h, service.h, types.h
│  │  │  ├─ thread.h, message_queue.h, task_runner.h, input_event.h   # Nema kernel
│  │  │  ├─ app/        app.h, app_context.h, app_host.h, app_host_manager.h,
│  │  │  │              app_manifest.h, app_registry.h, component_app.h
│  │  │  ├─ apps/       clock, counter, stopwatch, task_demo, ticker, wifi, bluetooth,
│  │  │  │              camera, touch_test, ui_showcase, js_app, js_app_store, embedded_apps
│  │  │  ├─ js/         js_engine.h (QuickJS-ng), nema_runtime_js.h (embedded JSX runtime)
│  │  │  ├─ ui/         canvas, node, widgets, layout, renderer, focus, hit_test,
│  │  │  │              component_runtime, component_screen, view_dispatcher, screen,
│  │  │  │              status_bar, virtual_keyboard, text_input, text_style, key, ...
│  │  │  ├─ input/      input_action.h, input_code.h, i_key_map.h, gesture.h,
│  │  │  │              i_touch_driver.h, pointer.h
│  │  │  ├─ hal/        display, buffer_display, async_display, battery, wifi, bluetooth,
│  │  │  │              camera, audio_input, audio_output, http_client, filesystem,
│  │  │  │              usb_cdc, remote_screen_tap, driver
│  │  │  ├─ link/       transport, klp_codec, klp_ble, ble_link_transport,
│  │  │  │              usb_cdc_link_transport, mux_transport, link_service
│  │  │  ├─ services/   clock, gui, input, audio, camera, cli, profile, remote,
│  │  │  │              display_power_manager
│  │  │  ├─ service/    service_container.h, service_manager.h
│  │  │  ├─ screens/    home, app_list, logs, settings, about, controls, lock,
│  │  │  │              profile_settings, sleep_settings, sounds_settings,
│  │  │  │              touch_settings, camera_settings, close_and_open_modal
│  │  │  ├─ system/     system_info.h, hardware_registry.h, capability_registry.h, board_profile.h
│  │  │  ├─ config/     config_store.h        crypto/ sha256.h        fs/ vfs.h, mem_filesystem.h
│  │  │  ├─ event/      event.h, event_bus.h, async_event_poster.h
│  │  │  └─ log/        logger.h, log_entry.h, log_sink.h, console_sink.h, memory_sink.h
│  │  ├─ src/...         (mirror of include/, plus js engine, font, thread_{host,esp32}.cpp)
│  │  └─ CMakeLists.txt  # dual: idf_component_register() OR add_library(nema_core STATIC)
│  │
│  ├─ platforms/
│  │  ├─ esp32/          Esp32Platform, clock, wifi, http_client, ble, usb_cdc,
│  │  │                  nvs_config_store, littlefs_filesystem
│  │  └─ wasm/           WasmPlatform, clock, config, null_display, wasm_cable_transport (KLP),
│  │                     sim_wifi_driver ("router")
│  │
│  ├─ boards/
│  │  ├─ simulator/      SimulatorBoard (virtual; BoardProfile only)
│  │  ├─ dev-board/      DevBoard, board_config.h, EinkDisplay (GxEPD2), TCA9534Buttons
│  │  └─ skyrizz-e32/    SkyRizzE32, board_config.h, TFT LCD + FT6336U touch + GC2145 camera
│  │                     + ES7243E mic + NS4168 speaker + XL9535 expander drivers
│  │
│  ├─ targets/
│  │  ├─ wasm/           Emscripten executable → nema.js + nema.wasm
│  │  ├─ dev-board/      ESP-IDF project (Arduino setup()/loop())
│  │  ├─ skyrizz-e32/    ESP-IDF project (USB-Serial-JTAG console)
│  │  ├─ skyrizz-camtest/   standalone camera + display bring-up test
│  │  └─ skyrizz-audiotest/ standalone mic + speaker bring-up test
│  │
│  ├─ vendor/            quickjs (JS engine), arduino-libs (GxEPD2/Adafruit_GFX/BusIO)
│  ├─ tests/             host unit tests: layout, klp, link, service, js (+ render/dualimport/graceful)
│  └─ tools/             build-wasm.sh, build/flash-dev-board.sh, build/flash-skyrizz-e32.sh, ...
│
├─ packages/
│  ├─ forge/             # SvelteKit web tool: /simulator (WASM), /remote (BLE/USB), /flash, /install
│  │  └─ src/lib/        transport/ (Ble/Serial/VirtualCable), klp/ (codec), RemoteSession, wasmSim
│  └─ nema-app-sdk/      # "nema" npm package: JSX components + hooks + nema-build CLI → .kapp
│     ├─ src/            components, jsx-runtime, render, hooks, system (nema.* API types)
│     ├─ bin/            nema-build.ts (TSX → .kapp)
│     └─ templates/      counter/, sysinfo/
│
├─ docs/
│  ├─ overview.md        # ← THIS FILE
│  ├─ STATE.md           # living state snapshot (Indonesian)
│  ├─ concept_plan.md    # master product vision
│  └─ plans/             # 00-overview + 01..41 per-subsystem implementation plans
│
└─ refs/                 # local-only reference projects (gitignored)
```

> `firmware/build/`, `firmware/build-wasm/`, `node_modules`, and `refs/` are gitignored.

---

## 7. Core Runtime

`Runtime` (`core/include/nema/runtime.h`, namespace `nema::`) owns every subsystem and assembles the
system. It is created via a static factory and driven by the target.

### Boot flow

The runtime advances through a `BootPhase` state machine (`types.h`):

```
None → PlatformLoaded → BoardLoaded → CoreReady → ServicesRegistered → Running
```

| Step | Call | What happens |
|---|---|---|
| 1 | `Runtime::create()` | construct the runtime |
| 2 | `loadPlatform(IPlatform&)` | platform provides clock + drivers → **PlatformLoaded** |
| 3 | `loadBoard(IBoard&)` | board provides hardware → **BoardLoaded** |
| 4 | `initCore()` | create Logger (+ console & 1024-entry memory sinks), EventBus, ServiceContainer, HardwareRegistry, CapabilityRegistry, SystemInfo, AppRegistry, AppHostManager, ViewDispatcher; publish **`SystemBoot`** → **CoreReady** |
| 5 | `registerServices()` | `platform.registerDrivers(rt)` → `board.describeHardware(rt)` → `platform.postRegister(rt)` (decorates the display with `RemoteScreenTap`, wires KLP transports); build ServiceManager; if `display` cap, create the `Canvas` (auto-scaled to logical pixels); publish **`SystemReady`** → **ServicesRegistered** |
| 6 | `start()` | `ServiceManager::startAll()`, start `TaskRunner` worker, start `GuiService` on its own thread → **Running** |
| 7 | target installs apps (built-in + embedded JS) and pushes `HomeScreen` | |
| 8 | `run()` / `loop()` | drive `step()` |

### The `step()` loop (one frame)

`step()` is the single source of timing. On WASM it is driven by `emscripten_set_main_loop`; on ESP32
the Arduino `loop()` calls it once per iteration. Each frame, **on the main task**:

```
1. asyncPoster_.flush(eventBus)     // drain cross-thread events FIRST (WiFi/BLE/HTTP completions)
2. serviceManager_.tickAll(now)     // tick all Running services
3. platform_.idle()                 // platform I/O (esp32: vTaskDelay; wasm: yield)
```

Rendering and input dispatch are **not** in `step()` anymore — they live on the dedicated
**GuiService thread** (see §9). The main task only pumps events and services.

Key accessors: `platform()`, `board()`, `clock()`, `log()`, `events()`, `container()`, `hardware()`,
`capabilities()`, `info()`, `asyncPoster()`, `input()`, `tasks()`, `apps()`, `appHost()`, `view()`,
`canvas()`, `dpm()`, `config()`, `audio()`, `camera()`, `fps()`, `phase()`, `exitCode()`.
Control: `requestShutdown()`, `requestRestart()` (sets exit code 75; the simulator auto-restarts on 75).

---

## 8. Core subsystems

### Logger (`log/`)
- **Levels:** `Trace, Debug, Info, Warn, Error, Fatal`.
- `Logger(IClock&)` → `log(level, component, msg, fields={})` + `info()/warn()/error()/…`.
- **Thread-safe** (a `std::mutex` guards `log()` and the sinks vector — fixes a dual-core race).
- **Sinks** (`ILogSink::write(const LogEntry&)`): `ConsoleSink` (stdout, human format), `MemorySink`
  (1024-entry ring buffer for the Logs screen + the KLP Log channel). On WASM no console sink is
  registered (no stdout in a Web Worker) — logs stream to Forge over KLP.
- `LogEntry { epochMs, level, component, message, fields[] }`.

### EventBus (`event/`)
- `Event { const char* name; std::vector<Field> payload; }`.
- `subscribe(name, handler) → SubscriptionId` (`"*"` = wildcard), `unsubscribe(id)`, `publish(event)`.
- **Synchronous dispatch** over a snapshot of the subscriber list (handlers may sub/unsub during dispatch).
- **Known event names** (`nema::events`): `SystemBoot`, `SystemReady`, `ServiceStarted/Stopped/Failed`,
  `ClockTick {uptimeMs}`, `BatteryChanged`, `NetworkConnected/Disconnected`, `WifiScanComplete`,
  `BtEnabled/BtDisabled/BtPairRequest/BtPaired/BtConnected/BtDisconnected`, `AppInstalled`, `AppRemoved`,
  `NotificationCreated` (reserved).

### AsyncEventPoster (`event/async_event_poster.h`)
- Thread-safe queue so **background threads** (WiFi/BLE/HTTP/worker) hand events to the main task:
  `post(Event)` from any thread, `flush(EventBus&)` from the main task only (top of `step()`).
- Backed by `MessageQueue<Event>`. The generic mechanism every off-thread producer reuses.

### Service system (`service/`, `service.h`)
- `IService { name(); start(); stop(); tick(nowMs){} }`.
- `ServiceContainer` — type-safe DI registry: `registerService<T>()`, `registerAs<I,T>()`,
  `addService()` (lifecycle-only), `resolve<T>()`, `require<T>()`. Preserves insertion order.
- `ServiceManager(container, log, bus)` — `startAll()` / `stopAll()` (reverse) / `tickAll(now)` /
  `startOne`/`stopOne`/`stateOf`. `ServiceState`: `Created → Starting → Running → Stopping → Stopped` /
  `→ Failed`; each transition publishes the matching `Service*` event.
- **Services:** `ClockService` (publishes `ClockTick`), `GuiService` (UI thread — render + input
  dispatch + FPS + sleep), `InputService` (thread-safe input funnel), `AudioService` (device registry),
  `CameraService` (device registry), `CliService` (Flipper-style command interpreter), `ProfileService`
  (device identity), `RemoteService` (KLP orchestrator), `DisplayPowerManager` (sleep/lock state machine).

### Introspection & config (`system/`, `config/`, `crypto/`, `fs/`)
- `SystemInfo` — `{ buildVersion, firmwareVersion, platformName, boardName, cpuMhz, ramKb, psramKb, flashKb }`.
  Versions come from a generated `version.h` (git hash + dirty flag; e.g. `0.1.0-dev+7e71458.dirty`).
- `HardwareRegistry` — `add(HardwareEntry{id, kind, detail})`, `has(DriverKind)`, `list()`.
- `CapabilityRegistry` — `add("wifi")`, `has("wifi")`, `list()`. The contract apps query.
- `BoardProfile` — physical layout (components with type/position/key) serialized to JSON for Forge.
- `IConfigStore` — namespaced key/value persistence (`getString/getInt/setString/setInt/remove`),
  NVS-backed on ESP32, in-RAM on host/WASM. Namespaces/keys ≤15 chars (NVS limit).
- `crypto/sha256.h` — `hexSha256()`, `randomHexSalt()` (used by `ProfileService` password hashing).
- `fs/` — `Vfs` composite filesystem (mount backends at paths, longest-prefix routing) + `MemFileSystem`
  (volatile RAM). On ESP32 a LittleFS backend mounts `/`; a MemFS mounts `/tmp`.

---

## 9. Nema kernel & threading model

The ESP32-S3 is **dual-core** with FreeRTOS underneath; WASM uses Emscripten **pthreads** (Web
Workers backed by a `SharedArrayBuffer`). The **Nema kernel** is the thin portable primitives layer
both targets share (`thread_esp32.cpp` / `thread_host.cpp` chosen at build time):

- **`Thread`** — `start(ThreadConfig{name, stackBytes, priority, core}, entry, arg)`, `requestStop()`,
  `shouldStop()`, `join()`, `sleepMs()`. On ESP32 → FreeRTOS task (with core affinity); on host → `std::thread`.
- **`MessageQueue<T>`** — thread-safe `send`/`receive(timeout)`/`tryReceive`; bounded or unbounded.
- **`TaskRunner`** — the anti-freeze worker pool: `submit(Job, Done)` runs `Job` on a worker thread
  (may block — WiFi scan, HTTP), then `Done` runs back on the UI thread via `drainCompletions()`.

### Thread layout

```
core 1 (UI)                     core 0 (work/IO)              main task (step loop)
──────────                      ────────────────              ─────────────────────
GuiService thread               TaskRunner worker             rt.step():
  owns Canvas + ViewDispatcher    scan(), http.get() (BLOCK)    asyncPoster.flush()
  drains InputService                                           serviceManager.tickAll()
  ticks screens, renders, flush  Input poll thread (board)      platform.idle()
  runs sleep/lock + FPS          → InputService queue

App thread (foreground app)
  IApp::run() — may BLOCK; draws to its own buffer; present() publishes a frame to the GUI thread
```

**Race-free by design:** the only cross-thread state is a pixel buffer (mutex-guarded, double-buffered
with a frame sequence counter so the latest frame is always eventually drawn) and thread-safe queues.
There is no shared mutable model. An app never touches the real `Canvas` or `ViewDispatcher`.

The three original data races are all closed: Logger sinks (mutex), off-thread event publishing
(`AsyncEventPoster`), and blocking display refresh (`AsyncDisplayDriver` + display task).

---

## 10. HAL — driver interfaces

All in `core/include/nema/hal/`. Concrete implementations live in platform/board layers.

```cpp
enum class DriverKind { Battery, Wifi, Bluetooth, Display, Storage, Other };

struct IDriver {                       // base for every driver
  const char* name() const; DriverKind kind() const; void onRegister(Runtime&) {}
};
```

| Interface | Purpose / key methods |
|---|---|
| **IDisplayDriver** | 1-bit mono framebuffer: `width/height`, `drawPixel`, `fillRect`, `clear`, `flush`, optional `invertRect`, `blitRgb565` (camera), `flushBuffer`, `sleep/wake`, `dpi()`. |
| **BufferDisplay** | RAM-backed display for off-screen rendering. |
| **AsyncDisplayDriver** | Wraps any display non-blocking (triple-buffer + task on ESP32; passthrough on host). Latest-wins. |
| **IBatteryDriver** | `level()` 0–100, `isCharging()`. |
| **IWifiDriver** | `connect/disconnect/isConnected/ssid`, `scan()` (blocking), `scanResults()`, `ip()`, `ipConfig()/setIpConfig()`. |
| **IBluetoothController** + **IBleAdapter** | Radio on/off/mode/name/address; GATT peripheral: register services, advertise, `notify`, `onWrite`, numeric-comparison pairing (`onPairRequest`/`confirmPairing`), bond list (`bondedAt`/`forget`). |
| **ICamera** | `label`, `frameWidth/Height`, `open/close`, `captureFrame()` → RGB565 buffer (blocking). |
| **IAudioInput** / **IAudioOutput** | `label`, `peakLevel()`; input `start/stopCapture`; output `setVolume`, `playTone(freq, ms)`. |
| **IHttpClient** | `get(url, insecure)` → `HttpResponse{status, body}` (blocking; call from a worker). |
| **IFileSystem** | `list/read/write/mkdir/remove` over `FsEntry{name, isDir, size}`; absolute `/` paths. |
| **IUsbCdc** | `isOpen`, `write`, `onData` — raw byte pipe (carries KLP). |
| **RemoteScreenTap** | An `IDisplayDriver` *decorator*: mirrors every flush as an RLE-encoded 1-bit frame onto the KLP Screen channel. Zero cost when no session is connected. |

The display contract is **1-bit monochrome** but **dimensions vary per board** (264×176 e-ink vs
240×320 TFT). The UI is resolution-independent; `Canvas` applies a logical-pixel scale so screens draw
from `canvas.width()/height()` and look right everywhere.

---

## 11. UI Runtime (retained-mode component tree)

The UI has moved from pure immediate-mode drawing to a **React-like retained-mode component tree**.
Each frame builds a tree of `UiNode`s, lays them out with flexbox, paints to the canvas, and routes
input — all from an arena that resets each frame (zero heap churn).

```
build(arena) → layout(root, metrics) → render(root, canvas, focused) → dispatch input
```

### Canvas (`ui/canvas.h`)
Wraps an `IDisplayDriver` with a **logical-pixel scale** (e.g. physical 528×352 at scale 2 → 264×176
logical). Primitives: `clear`, `drawPixel`, `fillRect`, `drawRect`, `drawLine`, `drawBitmap`,
`invertRect`, clip regions (`setClip`/`clearClip` for scroll viewports), `blitRgb565` (camera/video
fast path). Text: `setFont`, `drawChar/drawText`, `textWidth/Height`, scaled variants. Font `FONT_5X8`
(95 ASCII glyphs); larger text via integer pixel-doubling.

### Nodes, layout & widgets (`node.h`, `layout.h`, `widgets.h`)
- **Node types:** `View` (container), `Text`, `Pressable` (clickable, `onPress`), `Scroll` (viewport
  with persistent `ScrollState`), `Slider` (caller-owned `int*`).
- **Layout:** flexbox — `FlexDir{Row,Col}`, `Align`, `Justify`, `Style{flexGrow, width/height (SIZE_AUTO),
  padding, gap, align, justify, border, background}`. Two-pass: measure text → place nodes (absolute px).
- **`NodeArena`** — O(1)-reset pool; `alloc()` returns a zeroed node. **Builders:** `View`, `Text`,
  `Pressable`, `ScrollView`.
- **Component library:** `Row`/`Col`, `Container`, `Button`, `Header`/`Footer`, `ListRow`, `Menu`,
  `Modal`, and input controls `Toggle`, `Stepper`, `Select`, `Slider`, `TextField`.

### Component runtime & screens (`component_runtime.h`, `component_screen.h`)
`ComponentRuntime` gives every screen the same behavior for free: a **focus ring** (visible only in
button modality, like web `:focus-visible`), **auto-scroll** the focused node into view, **tap-vs-drag**
discrimination, **scroll momentum / flick**, and slider drag. A `ComponentScreen` subclass just
implements `build(arena, rt) → UiNode*`; lifecycle is `enter()/onAction()/onPointer()/draw()/tick()`.

### Screens, status bar, dispatcher (`screen.h`, `status_bar.h`, `view_dispatcher.h`)
- `IScreen`: `onAction(Action)`, `onCode(Code)`, `onPointer(PointerEvent)`, `update(Key)` (legacy),
  `draw(Canvas)`, `tick(ms)`, `mode()`, `modalWidth/Height`.
- `ScreenMode`: **Normal** (runtime draws the status bar + content area), **Fullscreen** (no status bar),
  **Modal** (previous screen + a centered box behind the modal's `draw()`).
- `StatusBar` is drawn centrally by the GUI thread (time `HH:MM`, battery %, WiFi, version) — centralizing
  + throttling it stopped the e-ink panel flashing.
- `ViewDispatcher` — screen stack: `push/pop/active/previous`, `handleAction/Code/Pointer/Key`, `tick`,
  and an **atomic** redraw flag (thread-safe; fixed a blank-screen race).

### On-screen keyboard & text (`virtual_keyboard.h`, `text_input.h`, `text_style.h`)
`VirtualKeyboard` — fullscreen QWERTY (Upper/Lower/Num+sym), `[CAPS]` / `[123/ABC]`, `[SPACE]/[OK]/[ESC]`,
password masking; `handle(Key)` for 2D nav, `handleAction(Action)` for 1D (3-button boards). `TextInput`
— modal 6-button character picker. `TextRole{Body,Title,Caption}` + `TextSize{Normal,Large}` drive font choice.

### Built-in screens (`screens/`)
Home, AppList, Logs, Settings (+ About, Controls, Sleep, Sounds, Touch, Camera, Profile sub-screens),
LockScreen, and the "Close & Open?" modal (single-app-slot policy). All but the modal are component-based.

---

## 12. Input abstraction

Input is layered so apps never care which physical buttons a board has.

- **`input::Action`** (the intent layer apps program against): floor actions **`Prev, Next, Activate,
  Back`** (every board must provide all four — `IKeyMap::validateFloor()` enforces it at boot), plus
  optional `AdjustUp, AdjustDown, Menu, Pause`. Screens implement `onAction(Action)`.
- **`input::Code`** — raw physical identity (`Up/Down/Left/Right/Enter/Escape/Menu`, custom ≥0x80). Use
  only when the physical key genuinely matters.
- **`Key`** (legacy 6-button enum `Up/Down/Left/Right/Select/Cancel`) — still used by the simulator and
  back-compat `update(Key)`. Conversion helpers bridge Key↔Code↔Action.
- **`input::Gesture`** — `Short, Long, Double, Chord, Repeat, Hold`. A per-board **`GestureEngine`**
  (tunable `longMs/repeatMs/holdMs/doubleMs`) turns raw button edges into gestures.
- **`IKeyMap`** — exactly one per board. Translates physical buttons (with gestures) into Code + Action,
  exposes `hintFor(Action)` (so footer labels are board-correct — never hardcode "Cancel"), `buttonLabel`,
  `canReach`, and `validateFloor()`.
- **Touch** — `ITouchDriver` emits `PointerEvent{phase: Down/Move/Up, x, y}` in **logical** canvas
  coordinates (orientation/scale/calibration applied in the driver). `InputModality` toggles the focus
  ring (Button) vs hidden (Pointer). `hitTest()` finds the pressable under a tap.
- Everything funnels through **`InputService`** as a single thread-safe `InputEvent` stream
  (Kind::Key or Kind::Pointer), drained by the GUI thread.

---

## 13. App model

Apps are the unit of functionality (it replaced the older `IPlugin`/`PluginManager` system).

### Interfaces (`app/`)
```cpp
struct IApp {
  const char* id();   const char* name();
  void run(AppContext& ctx);            // the app's main loop — runs on its OWN thread
  bool fullscreen();  uint32_t stackBytes();
};
```
- **`AppContext`** gives the app thread `canvas()` (its private draw buffer), `present()` (publish a
  frame to the GUI thread), `nextInput()`/`waitInput()` (its input mailbox), `requestExit()`,
  `shouldExit()`, and `runtime()`.
- **`AppHost`** is both an `IScreen` (GUI side) and the `AppContext` (app side). It spawns the app
  thread, double-buffers frames (mutex + sequence counter), forwards keys/pointers into the mailbox, and
  supports pause/resume (a paused app parks in `waitInput()` at ~0 CPU with its stack preserved).
- **`AppHostManager`** enforces a **single foreground app + at most one paused app**; launching a new
  app while one is paused pops the "Close & Open?" modal (models a memory-constrained device).
- **`AppRegistry`** / **`AppManifest`** — the installed-app table. `install()` (built-in),
  `installCustom()` (runtime JS apps), `installScreen()`/`installService()`; `AppKind{BuiltIn,Custom}`,
  `AppType{App,Service}`. The AppList screen launches from here.
- **`ComponentApp`** — base for declarative apps: implement `build(arena, ctx) → UiNode*` (+ optional
  `buildModal`, `onKey`, `onPointer`, `onTick`, `tickIntervalMs`, `capturesInput`, `drawRaw`). The runtime
  handles layout, focus, scrolling. Most built-in apps subclass this.

### Built-in apps (`apps/`)
| App | id | What it shows |
|---|---|---|
| **Clock** | `com.palanu.clock` | Live digital clock (status bar visible); ticks 250 ms. |
| **Counter** | `com.palanu.counter` | −/+/Reset buttons, focus nav, modal reset confirm. |
| **Stopwatch** | `com.palanu.stopwatch` | Fullscreen timer; Select run/stop, Up reset; ~50 ms ticks while running. |
| **Task Demo** | `com.palanu.taskdemo` | Spawns a 3 s "download" on `TaskRunner`; UI stays live. |
| **Ticker** | `com.palanu.ticker` | BTC/USD via HTTP on a worker; spinner; UI never freezes. |
| **WiFi** | `com.palanu.wifi` | Scan/pick/password/connect state machine; keyboard via `capturesInput`. |
| **Bluetooth** | `com.palanu.bluetooth` | BLE toggle, advertise, numeric pairing modal, bonded-device list. |
| **UI Showcase** | `com.palanu.uishowcase` | Gallery/test bench for Pressable/Scroll/Toggle/Stepper/Select/Slider. |
| **Touch Test** | `com.palanu.touchtest` | Fullscreen touch diagnostic (crosshair, zones, live x/y/phase). |
| **Camera** | `com.palanu.camera` | Live 1-bit viewfinder (`IScreen`): RGB565 → luminance threshold → bitmap. |

---

## 14. JavaScript apps (QuickJS + .kapp)

Palanu runs **sandboxed JavaScript/JSX apps** on an embedded **QuickJS-ng** engine — custom apps that
ship as portable bundles, no native binary per architecture.

- **`JsEngine`** (`js/js_engine.h`) — one `JSRuntime`/`JSContext` per app, on the app's thread. It
  resolves `import … from "nema"` to an **embedded JSX runtime** (`nema_runtime_js.h`), enforces a JS
  heap limit (4 MB, in PSRAM on ESP32), a per-frame deadline (~5 s runaway guard, checked every 500 µs),
  and a recursion/stack guard kept at 3/4 of the thread stack so a deep script throws cleanly instead of
  corrupting the real stack. `render(arena)` turns the JS component tree into a native `UiNode*` tree.
- **`JsApp`** (`apps/js_app.h`) — a `ComponentApp` that hosts a JS bundle; large stack (256 KB on ESP32,
  512 KB host). `build()` calls `engine.render()` each frame.
- **`JsAppStore`** (`apps/js_app_store.h`) — installs custom apps at runtime: `installApp(id,name,ver,js)`
  or `installKapp(bytes)`. Registers them as `AppKind::Custom` so they appear in the launcher immediately.
- **`.kapp` format** — a tiny container: `KAPP1\n<manifest-json>\n<minified-js-bundle>`. Installs are
  currently **volatile** (lost on reboot until on-flash persistence lands).
- **Embedded apps** (`apps/embedded_apps.h`) — two reference JS apps compiled into the firmware:
  `com.palanu.example.sysinfo` and `com.palanu.example.counter` (demonstrating `useState`, the `nema.*`
  API, and storage). `loadEmbeddedJsApps(rt)` installs them at boot.

The JSX runtime exposes components `View/Text/Pressable/ScrollView/Slider/Row/Col/Fragment` and hooks
`useState/useRef/useEffect`. App authors get this via the **`nema-app-sdk`** (§20).

---

## 15. KLP link layer & remote control

**KLP (Nema Link Protocol)** is a transport-agnostic binary protocol that multiplexes channels over a
single byte stream, so a host (PC/phone/Forge) can mirror the device screen, drive input, stream logs,
run CLI commands, browse files, and push apps — over BLE, USB, or a virtual cable.

- **Frame:** `[magic 0xAB][chan][flags][len:2 LE][payload][crc8]` (CRC-8/SMBus). 1-bit framebuffers are
  RLE-compressed.
- **Channels:** `Control` (handshake/ping), `Screen` (RLE framebuffer), `Input`, `Log`, `System`
  (GetInfo/Restart/Sleep/Shutdown), `Ota`, `Ext` (host→device sim control: InjectEvent / WifiSetNetworks /
  AppInstall), `Event` (EventBus stream), `Cli`, `File` (List/Read/Write/Mkdir/Remove).
- **Transports** (`ILinkTransport`): **BLE** (`BleLinkTransport` over a GATT service, MTU ~180),
  **USB-CDC** (`UsbCdcLinkTransport`, MTU 512), **Mux** (combine USB + BLE), and the **WASM virtual
  cable** (KLP over `postMessage`, MTU 16 KB). A loopback transport backs host tests.
- **`LinkService`** runs the handshake and routes decoded frames; app channels are gated until the
  handshake completes.
- **`RemoteService`** is the device-side orchestrator: it routes Input→`InputService`,
  System→power callback, Log→sink, Event→EventBus, CLI→`CliService`, File→`IFileSystem`, and serves the
  board profile JSON on `GetInfo`. **`RemoteScreenTap`** (an `IDisplayDriver` decorator) streams the screen.
- **`CliService`** is a Flipper-style command interpreter (`help`, `ram`, `hwinfo`, `caps`, `power`,
  `wlan`, `ble`, …) — not a Unix shell. Same commands work on every platform.

This is what makes the WASM simulator and a physical board **indistinguishable to Forge**: same protocol,
same services, only the transport differs.

---

## 16. Platform layer

A platform supplies the clock and concrete drivers and decides the output mode.

```cpp
struct IPlatform {
  const char* name(); IClock& clock(); OutputMode outputMode();   // Human | Json
  void registerDrivers(Runtime&);   // create + register drivers, add hardware/capabilities
  void postRegister(Runtime&);      // decorate drivers (wrap display in RemoteScreenTap, wire KLP)
  void idle();
};
struct IClock { uint64_t millis(); uint64_t epochMs(); };   // monotonic + wall-clock
```

### ESP32 platform (`platforms/esp32/`)
`Esp32Clock`, `Esp32WifiDriver` (STA + scan + NVS), `Esp32HttpClient` (esp_http_client + TLS, mbedTLS
cert bundle), `Esp32Ble` (NimBLE peripheral, declares `bluetooth.ble`), `Esp32UsbCdc` (native USB serial),
`NvsConfigStore` (NVS persistence), `LittleFsFileSystem` (mounts `/`) + MemFS (`/tmp`). `postRegister`
wires the KLP stack: it wraps the board display in `RemoteScreenTap` and muxes USB-CDC + BLE transports.
Display + buttons come from the **board** layer.

### WASM platform (`platforms/wasm/`)
The simulator is the firmware compiled with **Emscripten + pthreads**, running in the browser as Web
Workers. `WasmClock` (browser time), `WasmConfig` (in-RAM), `NullDisplay` (no glass — the screen streams
over the cable via `RemoteScreenTap`), `WasmCableTransport` (KLP over `postMessage`; JS delivers inbound
bytes via the exported `nema_nlp_recv`), and `SimWifiDriver` (a pure-C++ virtual "router"). MemFS only.
No stdio — all I/O goes over KLP to Forge.

---

## 17. Board layer

A board declares physical hardware (pins + chips). `IBoard { name(); describeHardware(Runtime&); profile(); }`.
The pin map lives in the board's `board_config.h` — the single source of truth.

### SimulatorBoard
Virtual 90×55 mm, 6 buttons, virtual LCD. Just a `BoardProfile`; hardware comes from the WASM platform.

### DevBoard (`boards/dev-board/`) — e-ink test rig
ESP32-S3-WROOM-1-N8R8 + 2.7″ e-ink (**GxEPD2_270_GDEY027T91**, 264×176) + 6 buttons via **TCA9534** @
0x20 (active-LOW) + **ATECC608B** @ 0x60. Pins: I²C `SCL=9/SDA=10`, button IRQ `1`; e-ink SPI
`SCK=11/MOSI=17/CS=12/DC=13/RST=14/BUSY=21`; power `PWR=18`, `SE_EN=8`. The e-ink driver runs the slow
(~1 s) refresh on a display task; `AsyncDisplayDriver` keeps the UI responsive (partial refresh, full
refresh on big changes / every ~30 updates to clear ghosting).

### SkyRizzE32 (`boards/skyrizz-e32/`) — multimedia flagship
ESP32-S3-WROOM-1-N16R8 (16 MB flash / 8 MB PSRAM) — a full audio/video badge:

| Subsystem | Hardware | Notes |
|---|---|---|
| Display | 240×320 TFT LCD over SPI (`SCLK=12/DC=13/CS=14/MOSI=21`, write-only) | backlight via XL9535 P00 |
| Touch | FT6336U / TSC2007 resistive, I²C, `PENIRQ=GPIO2` | logical coords |
| Camera | GC2145 DVP 8-bit parallel, `XCLK=GPIO7` 20 MHz | reset via XL9535 |
| Audio in | ES7243E I²S ADC mic | shared BCLK/WS |
| Audio out | NS4168 I²S speaker | |
| Buttons | 5 (Left/OK/Right + 2 side) via **XL9535** expander @ 0x20 (400 kHz) | OK long-hold = Back |
| Sensors | AHT20, LTR-303ALS (light), SC7A20 (accel), SE050 (secure element) | shared I²C |
| LED | WS2812 RGB on GPIO46 | |

I²C `SCL=48/SDA=47`; I²S `MCLK=3/BCLK=0/WS=38/DIN=39/DOUT=45`. The XL9535 also gates LCD backlight and
camera/touch/SE050 resets. (Console uses USB-Serial-JTAG, since UART0 pins are taken by the expander.)

---

## 18. Target layer

The target assembles platform + board + core into a flashable/runnable program.

| Target | Platform / board | Build | Entry | Output |
|---|---|---|---|---|
| **wasm** | WasmPlatform / SimulatorBoard | Emscripten (CMake) | `main()` + `emscripten_set_main_loop` | `nema.js` + `nema.wasm` |
| **dev-board** | Esp32Platform / DevBoard | ESP-IDF + arduino-esp32 | Arduino `setup()`/`loop()→rt.step()` | `nema-dev-board.bin` |
| **skyrizz-e32** | Esp32Platform / SkyRizzE32 | ESP-IDF + arduino-esp32 | `setup()`/`loop()` (USB-Serial-JTAG) | `nema-skyrizz-e32.bin` |
| **skyrizz-camtest** | — (bare) | ESP-IDF | direct camera + display test | bring-up binary |
| **skyrizz-audiotest** | — (bare) | ESP-IDF | direct mic + speaker test | bring-up binary |

ESP32 targets use **Arduino-as-component** under ESP-IDF v5.5.x (RTTI + exceptions on — the DI container
uses `std::type_index`). The WASM target links with fixed 512 MB memory, a 12-worker pthread pool, and
1 MB thread stacks (QuickJS needs the headroom). The two `*test` targets are standalone, runtime-free
programs for validating one subsystem on fresh hardware (direct register access, verbose serial output).

---

## 19. Forge — web simulator + remote/flash tool

`packages/forge/` is a **SvelteKit** app (Svelte 5, TailwindCSS 4 + shadcn-svelte, tRPC, Zod) — the
one web tool for both the simulator and real hardware. There is no WebSocket-to-C++ relay anymore: the
firmware is WASM, and everything speaks **KLP**.

- **Transports** (`src/lib/transport/`): `VirtualCableTransport` (postMessage to the WASM worker),
  `BleTransport` (Web Bluetooth → device GATT), `SerialTransport` (Web Serial → USB-CDC). All emit/consume
  the same KLP frames decoded by `src/lib/klp/codec.ts` (byte-for-byte identical to the C++ codec).
- **`RemoteSession`** abstracts a transport; `wasmSim.ts` is a shared singleton WASM instance.
- **Routes:** `/simulator` (run the WASM firmware — screen, logs, events, controls, a virtual WiFi
  router), `/remote` (discover & drive a Simulator/BLE/USB target), `/flash` (firmware flasher via
  **esptool-js** over Web Serial), `/install` (upload a `.kapp` over KLP). Because `/remote` can target
  the running `/simulator`, you can remote-control the same WASM instance you're simulating.

---

## 20. nema-app-sdk

`packages/nema-app-sdk/` is the **`nema`** npm package authors use to write custom apps in TSX, plus a
build CLI that bundles them to `.kapp`.

- **Author API:** components `View`, `Text`, `Pressable`, `ScrollView`, `Slider`, `Row`, `Col`; hooks
  `useState`, `useRef`, `useEffect`; flex styling props. A capability-gated ambient API: `nema.log(...)`,
  `nema.device.{name,caps,has()}`, `nema.storage.{get,set,remove}`, `await nema.http.get(url)` — each
  method is present only if the board declares the matching capability.
- **Workflow:** write `App.tsx` + a manifest → `nema-build <dir>` (esbuild → minified bundle) → a single
  `.kapp` file (e.g. ~700 B for the counter). Install it two ways: **embed** (a script turns `.kapp` into
  a C header compiled into firmware) or **OTA** (push over KLP from Forge `/install` — live, volatile).
- **Templates:** `counter/` and `sysinfo/` (the same two that ship embedded in firmware).

The SDK's component/hook surface mirrors the firmware's embedded JSX runtime, so an app renders through
the exact same native `UiNode` tree as built-in apps.

---

## 21. Build & run

Prerequisites: **Bun**; **Emscripten** (simulator); **ESP-IDF v5.5.x** (devices).

```bash
bun install

# ── Simulator (WASM in the browser, via Forge) ──
bun run forge:wasm        # build firmware → nema.wasm, launch Forge → open /simulator
bun run test              # host unit tests (layout / klp / link / service / js) via ctest

# ── Real boards (ESP32-S3) ──
bun run build:skyrizz-e32 && bun run flash:skyrizz-e32   # TFT multimedia badge
bun run build:dev-board   && bun run flash:dev-board     # e-ink dev board

# ── Hardware bring-up tests (standalone, no runtime) ──
bun run flash:camtest     # camera + display
bun run flash:audiotest   # mic + speaker
```

`firmware/tools/` holds the scripts: `build-wasm.sh` runs `emcmake cmake` + builds the `nema` target and
copies `nema.{js,wasm}` into Forge; the ESP-IDF scripts source `~/esp/esp-idf`, `idf.py set-target
esp32s3 build`, then `flash monitor`. Versioning is generated at configure time by
`firmware/cmake/nema_version.cmake` (parses `firmware/VERSION`, appends the short git hash + dirty flag;
**Release Please** automates the version bump + changelog + release on push to `main`).

---

## 22. Locked architecture decisions

| Topic | Decision |
|---|---|
| Core language | **C++17**, namespace **`nema::`**; no platform-specific includes in `core/**`. |
| Build | **CMake** for host/WASM, **ESP-IDF** for devices; `core/` is a dual-build component (`thread_host.cpp` vs `thread_esp32.cpp`). |
| Simulator | The firmware compiled to **WASM** (Emscripten + pthreads), running in the browser. No native binary, no server, no stdio. |
| Device ↔ host link | **KLP** (binary, channel-multiplexed) over BLE / USB-CDC / WASM virtual cable. Same protocol everywhere. |
| Web tool | **Forge** — SvelteKit + tRPC + esptool-js (no Vite/Express/ws relay). |
| Concurrency | UI on the `GuiService` thread; each app on its own thread; blocking work on `TaskRunner`; cross-thread state is only a mutex'd pixel buffer + queues. |
| Display | **1-bit monochrome**, dimensions per board; resolution-independent UI (logical-pixel `Canvas` scale). |
| UI | Retained-mode `UiNode` tree (flexbox), shared by C++ component apps and JS/JSX apps. |
| Input | Hardware-agnostic `Action` intents (+ gestures + touch); each board maps physical buttons via one `IKeyMap`. |
| Extensibility | **App model** (`IApp`, own thread) + sandboxed **QuickJS** `.kapp` apps; the old plugin runtime is retired. |
| Capability-driven | Apps check `capabilities().has("x")`, never the board type. |
| JSON in C++ | Vendored `nlohmann/json.hpp`, used by platform/target only (never core). |

---

## 23. Roadmap & plan status

The master vision is in `docs/concept_plan.md`; per-subsystem plans are `docs/plans/NN-*.md` (00–41).
Broadly, everything below is implemented (build + simulator verified; HW where noted):

- **M1–M5 — Core, observability, simulator, UI, app registry** (plans 01–15) — ✅ done.
- **M6 — ESP32 dev board + e-ink** (16–18) — ✅ runs on device.
- **M6.5–M6.8 — Async display/event foundation, Nema kernel, app model** (19, 19.5, 19.6) — ✅ (kernel
  F0–1 + TaskRunner on HW; app model HW-verified to the foreground-app stage).
- **M7 — Connectivity & UX** (20 WiFi, 21 sleep/lock, 22 pause/resume, 23 HTTP/keyboard/ticker, 24 config,
  25 adaptive UI, 26 simulator overhaul) — ✅ build + sim.
- **M8 — Component UI & media** (27 component UI, 29 touch, 30 component runtime/migration, 31 scroll/
  gestures, 32 media HAL, 33 board profile) — ✅.
- **M9 — Boards, connectivity stack, ecosystem** (28 SkyRizz E32, 34 BLE/USB, 35 KLP remote, 36 Forge,
  37 JS custom apps, 40 user profile) — ✅ build (SkyRizz E32 HW bring-up in progress).

**Still ahead:** plan 38 (persistent on-flash app storage / app store — currently `.kapp` installs are
volatile), plan 39 (firmware OTA + secure boot), plan 41 (the Palanu rebrand — namespace/text migration),
and full hardware verification of the connectivity + media stack on SkyRizz E32.

> The plan files are written in Indonesian and use ✅ done / 🚧 WIP / 📝 planned / ☐ not-started markers;
> consult `docs/STATE.md` and `docs/plans/00-overview.md` for the authoritative living status.

---

## 24. Known gaps

- **`.kapp` installs are volatile** — OTA-installed JS apps live in RAM and are lost on reboot until
  on-flash persistence (plan 38) lands. Embedded JS apps (compiled in) survive.
- **SkyRizz E32 is build-verified, HW bring-up ongoing** — the board/target compile and the camtest/
  audiotest programs exist; full runtime validation on the physical badge is in progress.
- **Connectivity not fully HW-verified** — WiFi/Ticker/keyboard/BLE build on all targets and pass in the
  simulator; end-to-end testing on the e-ink device is still pending in places.
- **No battery monitoring on the dev board** — no battery ADC in its pinout; the `battery` capability is
  simulator-only there.
- **ESP32 TLS** uses the mbedTLS cert bundle; the `insecure` flag only skips the common-name check.
- **Bluetooth Classic** (`IClassicAdapter`) is interface-only; ESP32-S3 has no classic radio (BLE only).
- **The rebrand is mid-flight** — the canonical name is **Palanu** and the namespace is **`nema`**, but
  the repo directory (`kairo/`) and the `README.md` still carry the older "Kairo" name, and the link
  protocol is still spelled `KLP` in code (target name: NLP). Plan 41 finishes reconciling these.

---

## 25. Glossary

| Term | Meaning |
|---|---|
| **Palanu** | The product / firmware runtime / ecosystem (canonical name; older name was "Kairo"). |
| **Nema** | The kernel layer + the C++ namespace `nema::` all firmware lives in. |
| **Core** | The portable, hardware-agnostic C++ runtime (`firmware/core`). |
| **Platform** | A concrete runtime environment: clock + drivers (`esp32`, `wasm`). |
| **Board** | A physical hardware definition: pins + attached chips (`simulator`, `dev-board`, `skyrizz-e32`). |
| **Target** | The buildable entry point that assembles Platform + Board + Core. |
| **Driver** | A concrete hardware implementation behind a HAL interface (e.g. `EinkDisplay`, `Esp32Ble`). |
| **Capability** | A named feature flag (`wifi`, `display`, `audio.input`, …) apps query instead of board type. |
| **Service** | A lifecycle-managed component (`start/stop/tick`) in the container. |
| **App** | An `IApp` that runs on its own thread; built-in (C++) or custom (JS `.kapp`). |
| **Screen** | An `IScreen` pushed onto the `ViewDispatcher` stack. |
| **KLP** | Nema Link Protocol — the binary remote-control wire protocol. |
| **Forge** | The web tool: WASM simulator + remote control + firmware flasher. |
| **.kapp** | A bundled custom JavaScript app (`KAPP1` + manifest + minified JS). |
| **SkyRizz E32** | The flagship ESP32-S3 multimedia board (TFT + touch + camera + audio). |

---

*For the living state snapshot see [`STATE.md`](STATE.md); for the full product vision see
[`concept_plan.md`](concept_plan.md); for per-subsystem implementation detail see
[`plans/`](plans/00-overview.md).*
