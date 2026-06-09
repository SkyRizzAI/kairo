# Kairo

**A hardware-agnostic firmware runtime for portable multi-tool devices.**

Kairo is one firmware core that runs on many boards. Write an app once — it runs
on every device, in the browser simulator, and on real hardware, without
touching the app for each new board. Bring a new device to life by writing
*drivers*, not a new firmware.

Think of the pocket multi-tools that hackers and tinkerers love — but built as
an **open ecosystem**: standardized apps, a consistent UI, a real driver model,
and a simulator so you can build without owning the hardware. The goal isn't to
clone the genre — it's to give it a foundation good enough to outgrow it.

---

## Why Kairo

- **One core, many boards.** App and UI code never branch on board type. They
  ask the runtime *"do you have a camera?"* — not *"are you board X?"*
- **Drivers, not forks.** A new device is a folder of drivers + a pin map. The
  core, the apps, the settings screens, and the UI come for free.
- **Simulate everything.** The whole firmware runs in a web/CLI simulator. Build
  and demo an app with zero hardware, then run the same core logic on a board.
- **Standardized apps & UI.** Apps target a stable app model and a
  resolution-independent UI. The same app looks right on a 240×320 LCD and a
  264×176 e-ink panel.
- **Capability-driven.** Features light up based on what the hardware declares.
  Plug in a board with audio → the Sounds screen appears. No audio → it's hidden.
  Nothing to configure.

---

## Architecture

Four layers, each depending only on the one below it:

```
  targets/      buildable apps          (skyrizz-e32, dev-board, simulator)
     |
  boards/       drivers + pin map       (skyrizz-e32, dev-board)
     |
  platforms/    SoC integration         (esp32, simulator)
     |
  core/         hardware-agnostic        (runtime, HAL, UI, apps, services)
```

- **core** (`firmware/core/`) — no hardware dependencies. The runtime, the UI
  toolkit, the app model, services, and the **HAL interfaces** that hardware
  plugs into.
- **platforms** (`firmware/platforms/`) — SoC-level integration (ESP32, or the
  host simulator).
- **boards** (`firmware/boards/`) — the **drivers** for a specific device, plus
  its pin map (the single source of truth).
- **targets** (`firmware/targets/`) — the actual flashable/runnable projects.

### Subsystems

Every hardware feature is a **subsystem** with the same shape. The core provides
the contract; a board provides the driver:

| Subsystem | Core provides (HAL interface) | A board provides (driver) |
|---|---|---|
| Display   | `IDisplayDriver` + `Canvas`        | e-ink, TFT LCD, … |
| Input     | `IKeyMap` + `Action` intents       | buttons, touch, gestures |
| Audio     | `IAudioInput` / `IAudioOutput`     | mic ADC, speaker amp |
| Camera    | `ICamera` + `CameraService`        | DVP / parallel sensors |
| WiFi      | `IWifiDriver`                      | SoC radio |
| HTTP      | `IHttpClient`                      | SoC stack |
| Battery   | `IBattery`                         | fuel gauge |

The pattern is identical across all of them: **learn it once, add any hardware.**

---

## Adding a new board

You implement drivers — you never modify core.

1. **Create `firmware/boards/<name>/`** with a `board_config.h` pin map (mirror
   the hardware doc — no magic numbers scattered in drivers).
2. **Write a driver** for each subsystem your hardware supports, implementing the
   core **HAL interface** (e.g. `IDisplayDriver`, `IAudioInput`, `ICamera`).
3. **Register + declare capabilities** in `describeHardware()`:

   ```cpp
   void MyBoard::describeHardware(Runtime& rt) {
       lcd_.init(rt, expander_);
       rt.container().registerService(&lcd_);
       rt.container().registerAs<IDisplayDriver>(&lcd_);
       rt.capabilities().add("display");        // the Display UI now works

       mic_.init(rt, expander_);
       rt.audio().addInput(&mic_, "mic0", "Built-in Mic");
       rt.capabilities().add("audio.input");    // the Sounds screen now appears
   }
   ```

4. **Add a target** in `firmware/targets/<name>/` and build.

Everything else — the home screen, app list, settings, the apps themselves —
already works, because none of it knows your board exists. It only checks
capabilities.

### Rules that keep the ecosystem coherent

- **Check capabilities, never board type.** Use `rt.capabilities().has("wifi")`,
  never `#ifdef` or a board-name branch in core/app code.
- **Program against intents, not buttons.** Apps handle `Action::Prev/Next/
  Activate/Back`; each board maps its physical buttons (with short/long/double/
  chord gestures) to those intents. Footer hints come from
  `rt.input().hintFor(Action)` — so labels are always right on every board.
- **Resolution-independent UI.** Draw from `canvas.width()/height()`; never
  hardcode screen dimensions.
- **One logger.** All logging goes through `rt.log()`, which fans out to the
  console and an on-device log ring buffer.

See [`CLAUDE.md`](CLAUDE.md) for the full project conventions.

---

## Built-in apps

Apps ship with the core and run on any capable board: Clock, Stopwatch, Counter,
Ticker (async HTTP), Task Demo, WiFi, Touch Test, and Camera. Settings cover
Display (sleep/lock/scale), Controls, Sounds (mic/speaker levels + test tone),
Camera, and a live Logs viewer.

---

## Getting started

Requires [Bun](https://bun.com). Firmware builds for ESP32 targets use ESP-IDF
(v5.5.x).

```bash
bun install
```

### Run in the simulator (no hardware needed)

The simulator is the firmware compiled to **WebAssembly**, running fully in the
browser via Kairo Forge (no native binary, no server).

```bash
bun run forge:wasm   # build firmware → WASM + launch Forge (open /simulator)
bun run test         # host unit tests (layout / KLP / link) via ctest
```

### Build / flash a real board

```bash
bun build:skyrizz-e32     # build firmware for the SkyRizz E32
bun flash:skyrizz-e32     # flash + monitor

bun build:dev-board       # the e-ink dev board
bun flash:dev-board
```

### Hardware bring-up tests

Standalone, self-contained programs for validating a single subsystem on new
hardware (no runtime, direct register access, verbose serial diagnostics):

```bash
bun flash:camtest         # camera + display bring-up
bun flash:audiotest       # mic + speaker bring-up
```

---

## Supported boards

| Board | SoC | Display | Highlights |
|---|---|---|---|
| **SkyRizz E32** | ESP32-S3 | 240×320 TFT LCD | 3-button + touch, mic, speaker, camera, sensors, RGB |
| **Dev Board**   | ESP32-S3 | 264×176 e-ink   | 6-button, NVS config, WiFi |
| **Simulator**   | host     | web / CLI canvas | full runtime, no hardware |

---

## Project layout

```
firmware/
  core/        hardware-agnostic runtime, HAL, UI, apps, services
  platforms/   esp32, simulator
  boards/      skyrizz-e32, dev-board
  targets/     buildable projects (+ bring-up tests)
  tools/       build/flash scripts
  tests/       host-side unit tests
packages/
  simulator/   web simulator UI
docs/
  plans/       design docs, one per subsystem/feature
```

---

## License

See [`LICENSE`](LICENSE).
