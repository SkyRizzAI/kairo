# Kairo — Project Overview

> **A single-file snapshot of the whole project.** Read this and you should understand what Kairo
> is, how it is built, what works today, how the code is organized, and where it is going — without
> opening any other file.
>
> **Last updated:** 2026-05-31 · **Status:** MVP + Plugin Runtime + UI Runtime + ESP32 dev board, all running on real hardware.

---

## Table of Contents

1. [What is Kairo](#1-what-is-kairo)
2. [Current status (verified)](#2-current-status-verified)
3. [Architecture at a glance](#3-architecture-at-a-glance)
4. [Hardware tiers](#4-hardware-tiers)
5. [Repository structure](#5-repository-structure)
6. [Core Runtime](#6-core-runtime)
7. [Core subsystems](#7-core-subsystems)
8. [HAL — driver interfaces](#8-hal--driver-interfaces)
9. [UI Runtime](#9-ui-runtime)
10. [Plugin system](#10-plugin-system)
11. [Platform layer](#11-platform-layer)
12. [Board layer](#12-board-layer)
13. [Target layer](#13-target-layer)
14. [Simulator web app + bridge protocol](#14-simulator-web-app--bridge-protocol)
15. [Threading & concurrency model](#15-threading--concurrency-model)
16. [Build & run](#16-build--run)
17. [Locked architecture decisions](#17-locked-architecture-decisions)
18. [Roadmap & plan status](#18-roadmap--plan-status)
19. [Known gaps](#19-known-gaps)
20. [Glossary](#20-glossary)

---

## 1. What is Kairo

Kairo is a **handheld device platform** in the spirit of Flipper Zero, built around a **portable C++17
Core Runtime** with a **1-bit retro/pixel UI**. The same Core runs unchanged in two environments:

- a **web simulator** (no hardware needed), and
- real **ESP32-S3 hardware with an e-ink display**.

The long-term goal is *not* merely to clone Flipper Zero, but to build a modern platform with a
**portable runtime, a strong plugin ecosystem, clean hardware abstraction, and simulator-first
development**.

### Guiding principles

| Principle | Meaning |
|---|---|
| **Simulator first** | Every feature should be developable & testable without hardware. |
| **Observability first** | The runtime must be easy to inspect (logs, events, services, registries). |
| **Plugin first** | Extra features ship as plugins, not core changes. |
| **Hardware agnostic** | Core must never know about a specific chip/display/board. |
| **Capability driven** | Apps check `capabilities.has("wifi")`, never `isEsp32()`. |

---

## 2. Current status (verified)

| Area | Status | Evidence |
|---|---|---|
| Core Runtime (boot, logger, event bus, services, introspection) | ✅ | runs on sim + ESP32 |
| Plugin Runtime (`IPlugin`, `PluginManager`, `PluginContext`) | ✅ | 4 plugins load/select/unload |
| UI Runtime (1-bit Canvas, 5×8 font, `ViewDispatcher`, screen stack) | ✅ | Home/Apps/Logs/Settings render |
| Built-in components (Label/Button/Row/Col/MenuItem/HRule, sizes xs–2xl) | ✅ | C++ + React mirror |
| Built-in screens (Home, AppList, Logs, Settings, About) | ✅ | navigable on sim + device |
| Simulator (native binary + Bun relay + React web UI) | ✅ | `bun run sim` |
| **Kairo Dev Board** (ESP32-S3 + e-ink GxEPD2 + 6 TCA9534 buttons) | ✅ | **builds, flashes, runs on physical device** |
| Modal dialogs, fullscreen screens | ✅ | CounterApp reset confirm, StopwatchApp |
| Non-blocking e-ink (`AsyncDisplayDriver` task + dirty-rect + latest-wins) | ✅ HW | no input freeze during refresh |
| **Nema kernel** (`nema::Thread`, `MessageQueue`, `TaskRunner`) | ✅ HW | F0–1 + TaskRunner on board |
| **Input thread** (TCA9534 poll on its own thread → InputService) | ✅ HW | fixes lost/jumped presses |
| **GuiService thread** (render + input dispatch off the main loop) | ✅ HW | flashed |
| **App-model** (`IApp`/`AppHost`/`AppContext`, each app = its own thread) | ✅ HW | Counter app-thread on board |
| All interactive apps → app-model (Clock/Counter/Stopwatch/TaskDemo/Ticker) | ✅ build | sim ✓, status bar in-app |
| **WiFi UI** (`WifiApp`: scan/pick/password/connect, non-blocking) | ✅ build | sim ✓ (HW pending) |
| **Virtual Keyboard** (QWERTY + CAPS + 123/sym + DEL/SPACE/OK/ESC + password) | ✅ build | sim render ✓ |
| **HTTP client HAL** (sim=curl, esp32=esp_http_client+TLS) | ✅ build | sim live Binance 200 |
| **Ticker app** (BTC/USD via Binance, fetch on worker, UI never freezes) | ✅ build | sim ✓ (HW pending) |
| Sim WiFi "router" (network list, per-net password/RSSI/online toggle) | ✅ | web panel + 4 scenarios ✓ |

The **same 6-button input** (`Up/Down/Left/Right/Select/Cancel`) and the **same 1-bit 264×176 display
contract** drive both the web canvas and the e-ink panel, so UI renders identically in both.

**The "never freeze" pillar is real:** WiFi scan (1–3s) and HTTP fetch (1–3s) run on a `TaskRunner`
worker thread while the UI keeps rendering and stays responsive. The reference firmware's own ticker
comment says it "freezes UI during fetch" — Kairo's does not.

---

## 3. Architecture at a glance

Kairo is built in four layers. Each layer only depends on the one above it through abstract interfaces.

```
   Core         portable C++17 runtime — knows NOTHING about hardware
    ▲           (Runtime, Logger, EventBus, Services, UI, Plugins, HAL interfaces)
    │ implements interfaces
 Platform       runtime environment: clock + drivers
    ▲           (simulator | esp32)
    │ provides
  Board          physical hardware definition: pins, attached chips
    ▲           (simulator | dev-board | kairo-board-v1*)
    │ assembled by
 Target          firmware entry point — main() / setup()+loop()
                (simulator | dev-board)
```

**The rule:** files under `firmware/core/**` may **never** `#include` anything platform-specific
(no `<Arduino.h>`, no ESP-IDF, no Bun, no `nlohmann/json`). Core only knows abstract interfaces
(`IPlatform`, `IBoard`, `IClock`, `IDriver`, `IDisplayDriver`, `IWifiDriver`, `IBatteryDriver`,
`IService`, `IScreen`, `IPlugin`). Everything hardware-specific lives in Platform/Board/Target.

**Why this matters:** moving from the Dev Board to the future custom V1 PCB only requires writing a
new *board layer* — the Core and the `esp32` platform are reused untouched.

---

## 4. Hardware tiers

Kairo officially recognizes three hardware tiers; the names are used consistently across code and docs.

| Tier | Name | `IBoard::name()` | What it is | Status |
|---|---|---|---|---|
| 1 | **Simulator** | `simulator` | Virtual; runs on host/web with dummy drivers. No hardware. | ✅ running |
| 2 | **Kairo Dev Board** | `dev-board` | *Temporary* real-hardware test rig: ESP32-S3-WROOM-1 (8 MB flash / 8 MB PSRAM) + 2.7″ e-ink 264×176 + 6 buttons via TCA9534. A repurposed prior-project device. | ✅ running on device |
| 3 | **Kairo Board V1** | `kairo-board-v1` | Custom in-house PCB — the final product target. Reuses the `esp32` platform + Core; only a new board layer needed. | ⏳ not yet designed |

> The master plan calls Tier 2 the "DevKit S3 Board"; in practice it is the **Kairo Dev Board**
> (an existing ESP32-S3-WROOM-1 + e-ink device).

---

## 5. Repository structure

This is a Bun monorepo (`bun` workspaces). `firmware/` is C++; `packages/` is the TypeScript/React web simulator.

```
kairo/
├─ package.json              # bun workspaces + top-level scripts (sim, build:esp32, flash:esp32)
├─ bun.lock
├─ tsconfig.json             # strict ESNext, bundler mode, react-jsx
│
├─ firmware/                 # ── all C++ ──
│  ├─ CMakeLists.txt         # top-level HOST build (C++17, -Wall -Wextra -Wpedantic)
│  │
│  ├─ core/                  # HARDWARE-AGNOSTIC runtime (dual-build: host + ESP-IDF), ~4.6k LOC
│  │  ├─ include/kairo/
│  │  │  ├─ runtime.h, board.h, platform.h, types.h, clock.h, service.h
│  │  │  ├─ log/        logger.h, log_entry.h, log_sink.h, console_sink.h, memory_sink.h
│  │  │  ├─ event/      event.h, event_bus.h, async_event_poster.h
│  │  │  ├─ service/    service_container.h, service_manager.h
│  │  │  ├─ services/   clock_service.h
│  │  │  ├─ system/     system_info.h, hardware_registry.h, capability_registry.h
│  │  │  ├─ hal/        driver.h, display.h, wifi.h, battery.h
│  │  │  ├─ plugin/     plugin.h, plugin_context.h, plugin_manager.h
│  │  │  ├─ plugins/    clock_plugin.h, counter_plugin.h, hello_plugin.h, stopwatch_plugin.h
│  │  │  ├─ ui/         canvas.h, components.h, key.h, screen.h, status_bar.h,
│  │  │  │              ui_constants.h, view_dispatcher.h
│  │  │  └─ screens/    home_screen.h, app_list_screen.h, logs_screen.h,
│  │  │                 settings_screen.h, about_screen.h
│  │  ├─ src/...         (mirror of include/, plus ui/font_5x8.cpp)
│  │  └─ CMakeLists.txt  # dual: idf_component_register() OR add_library(kairo_core STATIC)
│  │
│  ├─ platforms/
│  │  ├─ simulator/      HostClock, SimDisplay, SimWifiDriver, SimBatteryDriver,
│  │  │                  TelemetryBridge (JSON stdout), CommandReader (JSON stdin)
│  │  └─ esp32/          Esp32Platform, Esp32Clock, Esp32WifiDriver
│  │
│  ├─ boards/
│  │  ├─ simulator/      SimulatorBoard (no-op; hardware comes from platform)
│  │  └─ dev-board/      DevBoard, board_config.h (pinout), EinkDisplay (GxEPD2),
│  │                     TCA9534Buttons
│  │
│  ├─ targets/
│  │  ├─ simulator/      main.cpp (host executable)
│  │  └─ dev-board/      ESP-IDF project: main/main.cpp (Arduino setup()/loop()),
│  │                     sdkconfig(.defaults), partitions.csv, managed_components/ (vendored)
│  │
│  ├─ vendor/
│  │  ├─ nlohmann/       json.hpp (single-header; used by platform/target ONLY, never core)
│  │  └─ arduino-libs/   GxEPD2, Adafruit_GFX, Adafruit_BusIO (e-ink stack)
│  │
│  └─ tools/             build-sim.sh, run-sim.sh, build-esp32.sh, flash-esp32.sh
│
├─ packages/
│  └─ simulator/         # ── Bun relay + React web UI ──
│     ├─ index.ts        # Bun.serve: spawns kairo-sim, relays stdio ↔ WebSocket
│     ├─ frontend.tsx, index.html
│     ├─ lib/useSimSocket.ts      # WebSocket hook + state reducer
│     ├─ components/     DisplayPanel, ControlsPanel, LogsPanel, EventsPanel, ServicesPanel
│     └─ ui/             React mirror of native components (Label, Button, Row, Col, MenuItem, HRule)
│
├─ docs/
│  ├─ overview.md        # ← THIS FILE
│  ├─ STATE.md           # living state snapshot (Indonesian)
│  ├─ concept_plan.md    # master plan v0.5 (full product vision)
│  └─ plans/             # 00-overview + 01..21 per-stage implementation plans
│
└─ refs/                 # local-only reference projects (gitignored)
```

> `firmware/build/`, `node_modules`, `refs/`, and `compile_commands.json` are gitignored.

---

## 6. Core Runtime

`Runtime` (`core/include/kairo/runtime.h`) is the heart of the system. It owns every subsystem and
drives the main loop. It is created via a static factory and assembled by the target.

### Boot flow

The runtime advances through a `BootPhase` state machine:

```
None → PlatformLoaded → BoardLoaded → CoreReady → ServicesRegistered → Running
```

| Step | Call | What happens |
|---|---|---|
| 1 | `Runtime::create()` | construct the runtime |
| 2 | `loadPlatform(IPlatform&)` | platform provides clock + (later) drivers → **PlatformLoaded** |
| 3 | `loadBoard(IBoard&)` | board provides hardware descriptions → **BoardLoaded** |
| 4 | `initCore()` | create Logger (+ console & 1024-entry memory sinks), EventBus, ServiceContainer, HardwareRegistry, CapabilityRegistry, SystemInfo, PluginManager, ViewDispatcher → **CoreReady** |
| 5 | `registerServices()` | `platform.registerDrivers(rt)` then `board.describeHardware(rt)`; build ServiceManager; if `capabilities.has("display")` create the `Canvas`; publish **`SystemBoot`** → **ServicesRegistered** |
| 6 | `start()` | `ServiceManager::startAll()`; publish **`SystemReady`** → **Running** |
| 7 | target loads plugins + pushes `HomeScreen` | |
| 8 | `run()` / `loop()` | drive `step()` |

### The `step()` loop (one frame)

`step()` is the single source of timing. On the simulator `run()` calls it in a `while` loop; on
ESP32 the Arduino `loop()` calls it once per iteration. Each frame:

```
1. asyncPoster_.flush(eventBus)     // drain cross-task events FIRST (WiFi/BLE/NTP) → publish on main task
2. serviceManager_->tickAll(now)    // tick all Running services (e.g. ClockService)
3. pluginManager_->tickAll(now)     // tick all loaded plugins
4. update globalStatus_ (clock + wifi cap), throttled to once / 10s   // avoids e-ink flashing
5. viewDispatcher_->tick(now); if a redraw is pending:
      canvas.clear()
      Normal     mode → draw status bar, then active screen
      Modal      mode → draw previous screen + status bar, then white box + border, then modal screen
      Fullscreen mode → draw active screen only (no status bar)
      canvas.flush()
6. platform_->idle()                // sim: poll stdin + 5ms sleep · esp32: vTaskDelay(5ms)
```

Key accessors: `platform()`, `clock()`, `log()`, `events()`, `container()`, `hardware()`,
`capabilities()`, `info()`, `asyncPoster()`, `plugins()`, `view()`, `canvas()`, `phase()`, `exitCode()`.
Control: `requestShutdown()`, `requestRestart(exitCode=75)` (the simulator relay auto-restarts on exit 75).

---

## 7. Core subsystems

### Logger (`log/`)
- **Levels:** `Trace, Debug, Info, Warn, Error, Fatal`.
- `Logger(IClock&)` → `log(level, component, msg, fields={})` + convenience `info()/warn()/error()/…`.
- **Thread-safe:** a `std::mutex` guards `log()` and the sinks vector (fixed a dual-core data race).
- **Sinks** (`ILogSink::write(const LogEntry&)`):
  - `ConsoleSink` — stdout, format `[HH:MM:SS] [LEVEL] [Component] message key=value …` (Human mode only).
  - `MemorySink` — ring buffer (1024 entries) for introspection; always present.
  - `JsonStdoutSink` (sim platform) — emits structured JSON lines for the web UI.
- **Rule:** no direct `printf` anywhere — always go through the Logger.
- `LogEntry { epochMs, level, component, message, fields[] }`.

### EventBus (`event/`)
- `Event { const char* name; std::vector<Field> payload; }` where `Field` is a key/value pair.
- `subscribe(name, handler) → SubscriptionId` (name `"*"` = wildcard / all events), `unsubscribe(id)`, `publish(event)`.
- **Synchronous dispatch** using a snapshot copy of the subscriber list, so handlers may
  subscribe/unsubscribe during dispatch without corrupting iteration.
- **Known event names** (`kairo::events`): `SystemBoot`, `SystemReady`, `ServiceStarted`,
  `ServiceStopped`, `ServiceFailed`, `ClockTick` (`{uptimeMs}`), `BatteryChanged`,
  `NetworkConnected`, `NetworkDisconnected`; reserved future: `PluginLoaded`, `PluginUnloaded`,
  `NotificationCreated`.

### AsyncEventPoster (`event/async_event_poster.h`)
- Thread-safe queue so **background tasks** (WiFi/BLE/NTP/OTA on the second core) can hand events to
  the main task safely: `post(Event)` from any task, `flush(EventBus&)` from the main task only.
- Owned by `Runtime` as a value member; flushed at the very top of `step()`.
- This is the **generic mechanism** future drivers reuse instead of each rolling their own queue+mutex.

### Service system (`service/`, `service.h`)
- `IService { name(); start(); stop(); tick(nowMs){} }`.
- `ServiceContainer` — type-safe DI registry: `registerService<T>()`, `registerAs<I,T>()` (register
  under an interface), `resolve<T>() → T*`, `require<T>() → T&`. Preserves insertion order.
- `ServiceManager(container, log, bus, clock)` — lifecycle: `startAll()` (insertion order),
  `stopAll()` (reverse order), `tickAll(now)` (Running only), `stateOf(svc)`.
- `ServiceState`: `Created → Starting → Running → Stopping → Stopped`, or `→ Failed`. Each transition
  publishes the matching `Service*` event.
- **Built-in service:** `ClockService` publishes `ClockTick {uptimeMs}` roughly once per second.

### Introspection (`system/`)
- `SystemInfo` — `{ buildVersion, firmwareVersion, platformName, boardName, cpuMhz, ramKb, psramKb, flashKb }`.
- `HardwareRegistry` — `add(HardwareEntry{id, kind, detail})`, `has(DriverKind)`, `list()`.
- `CapabilityRegistry` — `add("wifi")`, `has("wifi")`, `list()`. Capabilities are the contract apps
  query (never the board type).

---

## 8. HAL — driver interfaces

All in `core/include/kairo/hal/`. Concrete implementations live in platform/board layers.

```cpp
enum class DriverKind { Battery, Wifi, Bluetooth, Display, Storage, Other };

struct IDriver {                       // base for every driver
  virtual const char* name() const = 0;
  virtual DriverKind  kind() const = 0;
  virtual void onRegister(Runtime&) {} // self-registration hook
};

struct IDisplayDriver {                // 1-bit monochrome, origin top-left, on=ink(dark)
  uint16_t width() const; uint16_t height() const;
  void drawPixel(x,y,on); void fillRect(x,y,w,h,on); void clear(on=false); void flush();
  void invertRect(x,y,w,h) {}                 // cursor highlight (optional)
  void flushBuffer(const uint8_t* buf, w, h); // raw 1-byte/pixel fast path (optional)
};

struct IBatteryDriver { int level() const /*0-100*/; bool isCharging() const; };

struct IWifiDriver { bool connect(ssid); void disconnect(); bool isConnected() const; const char* ssid() const; };
```

The display contract is fixed at **264×176, 1-bit** so the simulator canvas and the e-ink panel are
pixel-identical.

---

## 9. UI Runtime

The official UI standard. **Plugins never draw pixels directly** — they create `IScreen`s and use
the `Canvas` + built-in components. Rendering is **immediate-mode** (no retained widget tree; each
frame redraws from scratch) and **event-driven** (a frame only renders when a redraw is requested).

### Canvas (`ui/canvas.h`)
Wraps an `IDisplayDriver`. Primitives: `clear`, `drawPixel`, `fillRect`, `drawRect`, `drawLine`
(Bresenham), `drawBitmap` (MSB-first packed), `invertRect`, `flush`. Text: `setFont`, `drawChar`,
`drawText`, `textWidth/textHeight`, plus scaled variants `drawTextScaled(scale 1–4)`,
`textWidthScaled`, `centerX/centerXScaled`.

### Font
`FONT_5X8` (`ui/font_5x8.cpp`): 5×8 pixels, 95 ASCII glyphs (0x20–0x7E), column-major packing,
1-pixel inter-character spacing. Larger text is produced by integer pixel-doubling (scale 1–4).

### Key (`ui/key.h`)
The unified 6-button input across sim and hardware:
```
None, Up, Down, Left, Right, Select (confirm/OK), Cancel (back)
```
Helpers `keyName(Key)` / `keyFromName(str)`.

### IScreen + ScreenMode (`ui/screen.h`)
```cpp
struct IScreen {
  virtual void enter() {}                 // gained focus
  virtual void update(Key) {}             // handle input
  virtual void draw(Canvas&) = 0;         // render (pure)
  virtual void tick(uint64_t nowMs) {}    // periodic
  virtual ScreenMode mode() { return Normal; }
  virtual uint16_t modalWidth()  { return 210; }   // Modal only
  virtual uint16_t modalHeight() { return 64;  }
};
enum class ScreenMode { Normal, Fullscreen, Modal };
```
- **Normal** — runtime auto-draws the status bar above the content area.
- **Fullscreen** — whole 264×176 canvas, no status bar.
- **Modal** — runtime renders the previous screen + status bar, then a centered white box + border,
  then the modal's `draw()` (used for confirm dialogs).

### ViewDispatcher (`ui/view_dispatcher.h`)
A simple screen stack: `push(IScreen&)` (calls `enter()`, requests redraw), `pop()` (guards empty,
re-`enter()`s the revealed screen), `active()`, `previous()` (for the modal backdrop), `empty()`,
`handleKey(Key)` → `active()->update()`, `tick(now)`, and the redraw flag
(`requestRedraw()` / `takeRedraw()`). It does **not** own the main loop — `Runtime::step()` does.

### StatusBar (`ui/status_bar.h`)
`StatusBarData { hour, minute, battery, wifi, version }`. Rendered **centrally by the runtime** (not
by each screen): clock `HH:MM` on the left, `[W] BT [pct]%` on the right, separator line below.
Centralizing it + throttling updates to once / 10s was the key fix that stopped the e-ink panel from
flashing every second.

### Components (`ui/components.h`)
Immediate-mode building blocks with a size scale `XS, SM, MD, LG, XL, XXL` (mapping to pixel-doubling
factors 1–4):
- **Label** — text, size, `invert` (white-on-black).
- **Button** — text, size, `selected` (filled/inverted), padding.
- **RowLayout / ColLayout** — children laid out horizontally / vertically with a `gap`.
- **MenuItem** — text, `selected` (inverts row + `> ` prefix), optional right-aligned `hint`.
- **HRule** — horizontal line.

These are **mirrored 1:1 in React** (`packages/simulator/ui/`) so the web UI looks like the device.

### Built-in screens (`screens/`)
| Screen | Purpose | Navigation |
|---|---|---|
| **HomeScreen** | Root hub: big "KAIRO" logo + menu (Apps / Logs / Settings). Owns the three sub-screens. | Up/Down cycle, Select pushes the chosen screen. Never popped. |
| **AppListScreen** | Lists loaded plugins from `plugins().plugins()`; 8 visible rows, scrolls. | Up/Down scroll, Select → `selectPlugin(id)`, Cancel → back. |
| **LogsScreen** | Shows uptime + app count (full log viewer is a placeholder). | Cancel → back. |
| **SettingsScreen** | Capability-driven menu: WiFi (if cap), Bluetooth (if cap), About. Owns AboutScreen. | Up/Down, Select, Cancel. |
| **AboutScreen** | Board, platform, firmware version, uptime, capability list. | Cancel → back. |

---

## 10. Plugin system

Plugins are the unit of extensibility (`plugin/`).

### Interfaces
```cpp
struct IPlugin {
  PluginId id();   // e.g. "com.kairo.clock"
  const char* name(); const char* version();
  void onLoad(PluginContext&);            // subscribe events, register services, create screens
  void onUnload(PluginContext&);          // cleanup
  void onTick(PluginContext&, uint64_t);  // optional background work
  void onSelect(PluginContext&);          // optional: typically pushes the plugin's main screen
};
```
`PluginContext` gives a plugin scoped access to `log()`, `events()`, `capabilities()`, `container()`,
plus `pushScreen()/popScreen()` (guarded by the `display` capability), `requestRedraw()`,
`subscribe(name, handler)` (**auto-unsubscribed on unload** via RAII), and `registerService()`.

`PluginManager`: `load(IPlugin&)`, `unload(id)`, `unloadAll()` (reverse order), `tickAll(now)`,
`selectPlugin(id)` → `onSelect`, `isLoaded(id)`, `plugins()`. Publishes `PluginLoaded`/`PluginUnloaded`.

### Built-in plugins (`plugins/`)
| Plugin | id | What it demonstrates |
|---|---|---|
| **HelloPlugin** | `com.kairo.hello` | Minimal reference: subscribes to `ClockTick`, logs every 10s. |
| **ClockPlugin** | `com.kairo.clock` | A `Normal` screen showing `HH:MM:SS` + date; refreshes every 1s. |
| **CounterPlugin** | `com.kairo.counter` | Stateful counter + a **Modal** confirm dialog (Yes/No, defaults to No); inter-screen state sharing. |
| **StopwatchPlugin** | `com.kairo.stopwatch` | A **Fullscreen** timer; Select start/stop, Up reset; redraws every 50ms while running. |

Plugins are currently **statically instantiated** in the target's `main.cpp`. Dynamic loading from
SD/SPIFFS is future work.

---

## 11. Platform layer

A platform supplies the clock and the concrete drivers, and decides the output mode.

```cpp
struct IPlatform {
  const char* name();
  IClock& clock();
  OutputMode outputMode() { return Human; }   // Human or Json
  void registerDrivers(Runtime&) = 0;          // create + register drivers, add hardware/capabilities
  void idle() {}                               // called each frame
};
struct IClock { uint64_t millis(); uint64_t epochMs(); };  // monotonic + wall-clock
```

### Simulator platform (`platforms/simulator/`)
- **`HostClock`** — `steady_clock` for `millis()`, `gettimeofday()` for `epochMs()`.
- **`SimDisplay`** — virtual 264×176, 1 byte/pixel buffer; `flush()` emits a base64 frame as JSON.
- **`SimWifiDriver`** — synchronous: `connect()` immediately succeeds and publishes `NetworkConnected`.
- **`SimBatteryDriver`** — starts at 100%, drains 1% every 5s, publishes `BatteryChanged`.
- **`TelemetryBridge` + `JsonStdoutSink`** — outbound JSON lines (logs/events/services/snapshots/frames).
- **`CommandReader`** — non-blocking `poll()` on stdin; parses inbound JSON commands.
- `outputMode()` is `Json` when env `KAIRO_SIM_JSON` is set (the web relay sets it), else `Human` (CLI).
- `idle()` polls stdin (JSON mode) then sleeps 5ms.

### ESP32 platform (`platforms/esp32/`)
- **`Esp32Clock`** — `esp_timer_get_time()/1000` for `millis()`, `gettimeofday()` for `epochMs()`.
- **`Esp32WifiDriver`** — STA mode via ESP-IDF; registers `WIFI_EVENT`/`IP_EVENT` handlers that run on
  the `sys_evt` FreeRTOS task. Because that is a *different core/task* than the main loop, events are
  pushed onto an internal mutex-protected queue and drained in `tick()` on the main task (see §15).
- `outputMode()` is always `Human` (UART/Serial). `idle()` does `vTaskDelay(5ms)`.
- Display + buttons are registered by the **board** layer, not the platform.

---

## 12. Board layer

A board declares the physical hardware (pins, attached chips). `IBoard { name(); describeHardware(Runtime&); }`.

### SimulatorBoard
No-op augmentation — all virtual hardware is registered by the simulator platform.

### DevBoard (`boards/dev-board/`) — the real test rig
`describeHardware()` powers peripherals, brings up I²C, and registers the e-ink display + button
expander. All pins live in **`board_config.h`** (single source of truth):

| Function | Pins | Notes |
|---|---|---|
| Power / secure-element enable | `PWR=18`, `SE_EN=8` | HIGH = on |
| I²C (100 kHz) | `SCL=9`, `SDA=10`, `BTN_IRQ=1` | TCA9534 @ `0x20`, ATECC608B @ `0x60` |
| E-ink SPI | `SCK=11`, `MOSI=17`, `CS=12`, `DC=13`, `RST=14`, `BUSY=21` | |

**Buttons → Key** (TCA9534, active-LOW, inverted on read): `Left=bit0, Down=bit1, Up=bit2,
Right=bit3, Select=bit4, Cancel=bit5`.

- **`TCA9534Buttons`** (service) — polls the expander every ~50ms, debounces, and dispatches rising
  edges (press) to `rt.view().handleKey(k)` + redraw.
- **`EinkDisplay`** (`IDisplayDriver`) — panel **GxEPD2_270_GDEY027T91** (2.7″, 264×176, mono).
  Non-blocking: a dedicated FreeRTOS display task does the slow (~1s) SPI refresh while the main loop
  keeps running. It double-buffers (`draw_buf` written by main task, `flush_buf` sent by the task,
  `prev_buf` for dirty-rect diff), uses **partial refresh** for small changes, and forces a **full
  refresh** when >75% of the screen changed or every 30 partial updates (to clear ghosting). If the
  display task is still busy, `flush()` simply drops the frame (acceptable for e-ink).

> Note: Plan 19 (FreeRTOS Foundation) proposes extracting this threading into a generic
> `AsyncDisplayDriver` wrapper so the e-ink driver becomes plain synchronous SPI code — see §18.

---

## 13. Target layer

The target is the firmware entry point that assembles platform + board + core.

### Simulator target (`targets/simulator/main.cpp`)
Plain `int main()`: create platform/board → `loadPlatform`/`loadBoard` → `initCore` →
register `ClockService` → `registerServices` → `start` → load the 4 plugins → push `HomeScreen` →
`rt.run()` (blocking loop, reads stdin in JSON mode) → return `rt.exitCode()`.
Built by CMake, links `kairo_core` + `kairo_platform_sim` + `kairo_board_sim`.

### Dev-board target (`targets/dev-board/`)
An ESP-IDF project using **Arduino-as-component**. Globals hold the platform/board/runtime; Arduino
`setup()` runs the same boot sequence and `loop()` calls `rt.step()` once per iteration.
- **MCU:** ESP32-S3-WROOM-1-N8R8 (8 MB flash, 8 MB octal PSRAM @ 80 MHz, QIO).
- **Toolchain:** ESP-IDF v5.5.4 + arduino-esp32 v3.3.8 (kept in lockstep with the reference project),
  RTTI + C++ exceptions **on** (the DI container uses `std::type_index`).
- **Partitions** (`partitions.csv`): ~3 MB app (factory) + SPIFFS. `CONFIG_FREERTOS_HZ=1000` (1ms tick).
- Output binary ≈ 1.05 MB.

---

## 14. Simulator web app + bridge protocol

`packages/simulator/` is a Bun + React app. There is **no WebSocket in C++** — instead:

```
Browser (React) ──WebSocket /ws──► Bun.serve relay ──spawn + stdio──► kairo-sim (native C++)
       ▲                                  │   stdin: JSON commands
       └────────── WebSocket broadcast ───┘   stdout: JSON-lines telemetry
```

`index.ts` spawns `firmware/build/targets/simulator/kairo-sim` with `KAIRO_SIM_JSON=1`, line-parses
its stdout, and broadcasts to all WebSocket clients on a `"sim"` channel; browser commands are
relayed to the binary's stdin. Exit code 75 triggers an auto-restart. No Vite/Express/ws — pure Bun
(`bun --hot`) with `index.html` importing `frontend.tsx`.

### Telemetry — C++ → browser (JSON lines)
```jsonc
{ "type":"hello",     "binExists":bool }
{ "type":"log",       "ts":n, "level":"INFO", "component":"…", "message":"…", "fields":{…} }
{ "type":"event",     "ts":n, "name":"…", "payload":{…} }
{ "type":"service",   "name":"…", "state":"Running|Stopped|Failed|…" }
{ "type":"frame",     "width":264, "height":176, "data":"<base64 1-byte/pixel>" }
{ "type":"system",    "info":{ "platform":"…","board":"…","buildVersion":"…","firmwareVersion":"…" } }
{ "type":"hardware",  "items":[{ "id":"battery","kind":"Battery","detail":"…" }, …] }
{ "type":"capability","items":["battery","wifi","display", …] }
{ "type":"ready" }   { "type":"sim_exit","code":n }   { "type":"error","message":"…" }
```

### Commands — browser → C++
```jsonc
{ "cmd":"boot|shutdown|restart" }
{ "cmd":"press_key", "key":"Up|Down|Left|Right|Select|Cancel" }
{ "cmd":"wifi_connect", "ssid":"…" }   { "cmd":"wifi_disconnect" }
{ "cmd":"inject_event", "name":"…", "payload":{…} }
```

### Panels (React)
- **DisplayPanel** — decodes the base64 frame to a 264×176 1-bit image, renders pixel-perfect at 2× with
  three themes (E-Ink beige/black, Phosphor green, Amber).
- **ControlsPanel** — Boot/Shutdown/Restart, WiFi connect/disconnect, a 3×3 D-pad + Select/Cancel,
  and an event-injection form. Physical arrow keys / Enter / Esc also map to keys.
- **LogsPanel** — level-filtered, color-coded log stream (caps at 500).
- **EventsPanel** — event timeline with payloads (caps at 500).
- **ServicesPanel** — per-service state dots.
- **`ui/`** — React mirror of the native components (Label, Button, Row, Col, MenuItem, HRule) sharing
  the same 1-bit retro look via `--kairo-fg`/`--kairo-bg` CSS variables.

---

## 15. Threading & concurrency model

The ESP32-S3 is **dual-core** with FreeRTOS always running underneath. Kairo's main loop runs on one
task/core; ESP-IDF callbacks (WiFi/IP events) run on the `sys_evt` task on the other core. Three
real data races were identified and fixed:

1. **Logger** — `sinks_` vector was read while a background task could log. Fixed with a `std::mutex`
   in `Logger::log()`.
2. **WiFi events** — the WiFi event handler publishing straight to `EventBus` from `sys_evt` raced
   the main loop's dispatch. Fixed by enqueuing events on a mutex-protected queue and draining them
   on the main task in `tick()`.
3. **Display blocking** — a full e-ink refresh (~1–2s) froze input/plugins/events. Fixed with the
   dedicated display task + double buffering (see §12).

The **generic answer** to "background task needs to publish an event" is `AsyncEventPoster`
(`post()` from any task, `flush()` on the main task in `step()`). The simulator is single-threaded and
needs none of this. Plan 19 formalizes these patterns (generic `AsyncEventPoster` + `AsyncDisplayDriver`)
so future drivers (BLE, HTTP, OTA, NTP) never reinvent the queue/mutex/task plumbing.

---

## 16. Build & run

Prerequisites: **bun**, **cmake**, **clang** (simulator); **ESP-IDF v5.5.4** at `~/esp/esp-idf` (device).

```bash
# Install workspace deps
bun install

# ── Simulator (web UI) ──
bun run sim          # build core C++ → start Bun relay + web → open URL, click Boot
bun run sim:cli      # human-readable CLI mode (no web)
bun run sim:web      # just the web dev server (expects an existing binary)
bun run build:firmware   # only build kairo-sim

# ── Kairo Dev Board (ESP32-S3) ──
bun run build:esp32  # idf.py build → kairo-dev-board.bin (~1.05 MB)
bun run flash:esp32  # flash + serial monitor (board connected via USB)
```

What the scripts do (`firmware/tools/`): `build-sim.sh` configures CMake (`firmware/build`) and builds
the `kairo-sim` target; `run-sim.sh` execs the binary; `build-esp32.sh` sources ESP-IDF,
`idf.py set-target esp32s3` + `build`; `flash-esp32.sh` runs `idf.py -p <port> flash monitor`.

---

## 17. Locked architecture decisions

| Topic | Decision |
|---|---|
| Core language | **C++17**, namespace `kairo::`; no platform-specific includes in `core/**`. |
| Build | **CMake** for host (clang), **ESP-IDF** for device; `core/` is a dual-build component. |
| Sim ↔ web bridge | **No WebSocket in C++.** Binary speaks JSON-lines over stdin/stdout; Bun spawns it and relays to a WebSocket. |
| Web stack | `packages/simulator`, **Bun + React** (no Vite/Express/ws). |
| JSON in C++ | Vendored single-header `nlohmann/json.hpp`, used by platform/target only. |
| Display | **1-bit 264×176** to match the e-ink panel; event-driven render (flush only on `requestRedraw`). |
| Input | One 6-key `Key` enum shared by sim and hardware. |
| Capability-driven | Apps check `capabilities.has("x")`, never the board type. |
| ESP32 | arduino-esp32 v3.3.8 + ESP-IDF v5.5.4 (lockstep with ref), Arduino-as-component, RTTI+exceptions on, octal PSRAM, ~3 MB app partition. |

---

## 18. Roadmap & plan status

The master vision is in `docs/concept_plan.md`; per-stage plans are in `docs/plans/NN-*.md`.

| Phase / Milestone | Plans | Status |
|---|---|---|
| **M1–M3 — Core + Observability + Simulator (MVP)** | 01 repo/build · 02 runtime/boot · 03 logger · 04 event bus · 05 service container/manager · 06 HAL+sim platform · 07 board/target · 08 introspection · 09 stdio bridge · 10 web UI · 11 integration | ✅ all done |
| **M4 — Plugin Runtime** | 12 plugin runtime | ✅ |
| **M5 — UI Runtime** | 13 display HAL+sim · 14 UI runtime (Key/Screen/ViewDispatcher) · 14b components · 15 status bar + home screen | ✅ |
| **M6 — Kairo Dev Board (ESP32-S3 + e-ink)** | 16 esp32 platform · 17 dev board · 18 e-ink display | ✅ builds, flashes, runs |
| **M6.5 — Async Display & Event Foundation** | 19 (`AsyncEventPoster` + `AsyncDisplayDriver`, generic) | ✅ done, on HW |
| **M6.7 — Nema Kernel** | 19.5 (`nema::Thread`/`MessageQueue`/`TaskRunner` + Input thread) | ✅ F0–1 + TaskRunner on HW |
| **M6.8 — App Model** | 19.6 (GuiService thread + `IApp`/`AppHost`/`AppContext`, all apps migrated) | ✅ build; HW: Fase A,B |
| **M7 — Connectivity** | 20 wifi (`WifiApp` scan/connect non-blocking, sim "router", esp32 NVS) · 23 http+keyboard+ticker | ✅ build + sim; HW pending |
| **M7 — UX Polish** | 21 screen-sleep-lock · 22 app pause/resume | ☐ not started |
| **Future — Kairo Board V1 (custom PCB)** | schematic, board_config, bring-up, validation | ⏳ not designed |

### Strong candidates for next work
- **Flash & verify on HW** — WiFi end-to-end (scan → keyboard → connect), Ticker over real WiFi, keyboard on e-ink.
- **Plan 21** (screen sleep + lock) and **plan 22** (app pause/resume) — UX polish.
- **Nema 19.6 Fase D** — per-core tuning + crash-isolation demo.
- **Kairo Board V1** — design the PCB + a new board layer (reusing the `esp32` platform). The product direction.
- WiFi: static IP apply (esp_netif), multiple saved networks (NVS profiles).

---

## 19. Known gaps

- **Connectivity not HW-verified yet** — WiFi UI, Ticker, and the Virtual Keyboard build on both
  targets and pass in the simulator, but haven't been flashed/tested on the e-ink device.
- **App-model HW partial** — GuiService + app threads run on the board up to Fase B (Counter);
  migrating *all* apps (Fase C) is build/sim-verified only.
- **No battery monitoring on the Dev Board** — no battery ADC in the reference pinout; the
  `battery` capability is not registered on hardware (it exists in the simulator). Coming with V1.
- **ESP32 TLS** uses `esp_crt_bundle_attach`; the `insecure` flag only skips the common-name check.
- **`api.binance.com` is 404 in the dev region** — the Ticker uses `data-api.binance.vision`
  (the public mirror, same as the reference firmware).
- **Web panel tsconfig** lacks `lib:["dom"]`, so `tsc --noEmit` reports `window`/`document` errors.
  Pre-existing, not a regression; Bun's bundler builds fine regardless.
- **Plugins are static** — instantiated in `main.cpp`; no dynamic loading from storage yet.
- `refs/` and `firmware/build/` are local-only / gitignored; Arduino libs for the e-ink stack are
  vendored under `firmware/vendor/arduino-libs/`.

---

## 20. Glossary

| Term | Meaning |
|---|---|
| **Core** | The portable, hardware-agnostic C++ runtime (`firmware/core`). |
| **Platform** | Concrete runtime environment: clock + drivers (`simulator`, `esp32`). |
| **Board** | A physical hardware definition: pins + attached chips (`simulator`, `dev-board`, future `kairo-board-v1`). |
| **Target** | The firmware entry point (`main()` / `setup()+loop()`) that assembles Platform + Board + Core. |
| **Driver** | A concrete hardware implementation behind a HAL interface (e.g. `EinkDisplay`, `Esp32WifiDriver`). |
| **Bridge** | The stdio↔WebSocket link between the native simulator binary and the web UI. |
| **Capability** | A named feature flag (`wifi`, `display`, …) apps query instead of checking board type. |
| **Service** | A lifecycle-managed component (`start/stop/tick`) registered in the container. |
| **Plugin** | A loadable app/extension implementing `IPlugin`. |
| **Screen** | An `IScreen` view pushed onto the `ViewDispatcher` stack. |

---

*For the living state snapshot see [`STATE.md`](STATE.md); for the full product vision see
[`concept_plan.md`](concept_plan.md); for per-stage implementation detail see [`plans/`](plans/00-overview.md).*
