# Platforms, Boards & Targets

> How the hardware-free `core/` is bound to real (or simulated) hardware:
> `IPlatform` (chip/OS environment) + `IBoard` (physical wiring) composed by a `target`
> (the buildable project).

## Contracts

**`IPlatform`** (`firmware/core/include/nema/platform.h`):
- `name()`, `clock()` (pure virtual).
- `outputMode()` → `Human`/`Json`.
- `power(PowerAction)` (Restart/Shutdown/Bootloader) — default no-op; hardware overrides.
- `registerDrivers(Runtime&)` (pure) — register platform drivers/services.
- `postRegister(Runtime&)` — runs after the board's `describeHardware()` but **before Canvas
  binds**; used to decorate the board display.
- `idle()` — per-loop I/O poll. `syncNtp()` — from the worker thread (default false).

**`IBoard`** (`firmware/core/include/nema/board.h`):
- `name()`, `describeHardware(Runtime&)` (pure) — declare hardware/capabilities, register drivers,
  install the keymap.
- `profile()` → `const BoardProfile&` — physical layout for visualization/Forge
  (`system/board_profile.h`: id/name/dims + `ComponentDef[]` with type, normalized x/y/w/h, remote `Key`).
- `sdSpi(SdSpiConfig&)` → default false; boards with an SD socket return SCLK/MISO/MOSI/CS/hostId
  so the ESP32 platform mounts `/sd`. `SdSpiConfig` is plain ints (no IDF dependency — safe in core).

## ESP32 platform (`firmware/platforms/esp32/`)

`Esp32Platform` (`esp32_platform.*`). Drivers/services (each self-registers via its own
`onRegister(rt)`): `Esp32Clock`, `Esp32WifiDriver` (`net.wifi`), `Esp32HttpClient`, `Esp32Ble`
(NimBLE, `bluetooth.ble`), `Esp32UsbHid` (BadUSB, Plan 66), `Esp32OtaUpdater` (`esp_ota`, A/B
rollback, `confirmBoot()`), `NvsConfigStore` (`IConfigStore` over ESP-IDF NVS), `ProfileService`
(`profile`).

**PLP remote substrate** (built unconditionally in `postRegister`, display-independent):
`Esp32UsbCdc` + `BleLinkTransport` combined in a `MuxTransport` → one `LinkService` (Device role)
+ a `RemoteService`. Capabilities `remote.usb` (always) + BLE if the radio exists. CLI gets core
built-ins plus a live-heap `ram`.

**Filesystems / VFS** (Plan 38): `/` LittleFS (persistent `spiffs` partition; seeds `/apps`,
`/data`, `/badusb` with factory scripts + `/readme.txt`), `/tmp` Mem (volatile), `/sd` SdFat
(only if `sdSpi()` returns pins). **Display decoration**: if `caps::Display`, `RemoteScreenTap`
wraps the board display and re-registers as `IDisplayDriver`. **Power**: flush replies (200 ms)
then `esp_restart()` / GPIO0-low + `esp_cpu_reset` (download mode) / `esp_deep_sleep_start()`.

**USB modes** (see CLAUDE.md "USB mode toggle" and [ADR 0001](../decisions/0001-usb-jtag-remote-uses-hwcdc.md)):
toggled by 2 lines in `firmware/targets/skyrizz-e32/CMakeLists.txt`. **Uncommented** =
`ARDUINO_USB_CDC_ON_BOOT=1` → USB HID/CDC mode (CDC serial + HID keyboard; BadUSB; flash via USB
OTG/download mode). **Commented** (current) = JTAG/Serial mode (`Serial`=UART0, TinyUSB not
initialized; flash via built-in USB Serial/JTAG, faster). The PLP remote works in both modes — but
in JTAG mode the `IUsbCdc` impl must drive **HWCDC**, not `Serial`. Toggling requires a clean build.

## WASM platform (`firmware/platforms/wasm/`)

`WasmPlatform` (`wasm_platform.*`) — firmware in the browser, "no glass, no stdio": a pure PLP
endpoint over a virtual cable. Differences from ESP32: single `WasmCableTransport` (not a mux);
`NullDisplay` + `RemoteScreenTap` (all rendering streamed; the platform itself declares `display`
+ `input`); `WasmConfig` instead of NVS; `SimWifiDriver` (virtual router — Forge injects networks
via `WifiSetNetworks`); `SimOtaUpdater` (dry-run); VFS = two in-RAM `MemFileSystem`s (`/`, `/sd`).
Build emits `palanu.js` + `palanu.wasm` with pthreads (Web Workers); `main()` uses
`emscripten_set_main_loop`.

## Boards

### SkyRizz E32 — `firmware/boards/skyrizz-e32/` (active board)
- **Chip**: ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB Octal PSRAM).
- **Display**: TFT LCD over direct ESP32 SPI (`LcdDriver`); backlight via XL9535 P00. Cap `display`.
- **I/O expander**: XL9535 16-bit I²C @0x20 (`xl9535.cpp`) — 5 buttons, backlight, peripheral resets.
- **Input**: 3 buttons below LCD (Left / OK-Back / Right) + 2 side (Up/Down). Caps `input`,
  prev/next/activate/back/adjust, `input.2d` (5 buttons → grid keyboard).
- **Touch**: FT6336U capacitive @0x38 (`ft6336_touch.cpp`), cap `input.touch`.
- **Audio**: ES7243E mic @0x11 (I2S0, `audio.input`); NS4168 amp sharing I2S0 TX (`audio.output`).
- **Camera**: GC2145 2MP DVP @0x3C (`gc2145_camera.cpp`), cap `camera`.
- **Other caps** (registry only): RGB WS2812×2; env/light/motion sensors.
- **SD**: TF1 microSD on SPI3 (SCLK 44 / MISO 41 / MOSI 42 / CS 40, host 2) — shared bus with the
  GT30L24A3W font ROM (mutually-exclusive CS). Note: SCLK GPIO44 is UART0 RX — benign because the
  host link is HWCDC, not UART0 (see ADR 0001).
- **Pin map (source of truth)**: `include/nema/skyrizze32/board_config.h`.
- **Keymap**: `E32KeyMap` (`e32_key_map.cpp`) — side Up/Down = nav (Prev/Next), below-screen
  Left/Right = AdjustDown/Up, middle: tap=Activate, double=Back, long-hold=Pause. Passes `validateFloor()`.

### Palanu Dev Board — `firmware/boards/dev-board/`
- **Chip**: ESP32-S3-WROOM-1 (N8R8). **Display**: 2.7" e-ink GDEY027T91 264×176 (GxEPD2), raw
  `EinkDisplay` wrapped by `AsyncDisplayDriver` (non-blocking flush). **Input**: TCA9534 6-button
  expander @0x20 (`tca9534_buttons.cpp`). **Other**: power-rail gating (GPIO18), ATECC608B secure
  element @0x60. **Pin map**: `include/nema/devboard/board_config.h`. **Keymap**: `DevBoardKeyMap`.

### Simulator board — `firmware/boards/simulator/`
`SimulatorBoard` (`simulator_board.cpp`) — minimal; hardware is registered by the WASM platform.
`describeHardware` only logs + demonstrates the capability-check pattern. Used by the WASM target.

## Targets (`firmware/targets/*`)

Each ESP-IDF target is an arduino-as-component project: `CMakeLists.txt` lists `EXTRA_COMPONENT_DIRS`
(core, esp32, the board, vendor quickjs/wasm3); `main/main.cpp` instantiates platform + board +
runtime and runs the standard boot.

| Target | What it is | Build (bun) |
|---|---|---|
| **dev-board** | Full firmware: Esp32Platform + DevBoard + Clock/Dolphin/JS apps + HomeScreen | `build:dev-board` (alias `build:esp32`) / `flash:dev-board` |
| **skyrizz-e32** | Full firmware: Esp32Platform + SkyRizzE32 + Hello/BadUsb/Dolphin/JS apps + HomeScreen | `build:skyrizz-e32` / `flash:skyrizz-e32` |
| **skyrizz-camtest** | Standalone bring-up (**no runtime**): ILI9341 + GC2145 diagnostics, raw `printf` | `build:camtest` / `flash:camtest` |
| **wasm** | Browser simulator: WasmPlatform + SimulatorBoard; emits `palanu.js`/`palanu.wasm` into Forge | `build:wasm` (needs emsdk) / `forge:wasm` |

ESP32 build scripts (`firmware/tools/build-*.sh`) source `$IDF_PATH` (default `~/esp/esp-idf`),
run `idf.py set-target esp32s3` on first build, then `idf.py build`. `sdkconfig.defaults` sets
arduino autostart, RTTI+exceptions, 16 MB flash with `partitions.csv` (A/B OTA), Octal PSRAM,
enlarged task stacks, FATFS LFN/512-byte sectors for the microSD.

## Conventions & gotchas

- **Capabilities, never board type** — branch on `rt.capabilities().has("display"|"net.wifi"|
  "input.2d"|…)`, never on board name or `#ifdef ESP32`. Boards declare caps in `describeHardware`;
  the platform adds infra caps (`storage`, `remote.usb`, `profile`).
- **Pin map = single source of truth** — every pin/address/port-bit lives in the board's
  `board_config.h`; drivers reference those constants. (The bring-up test targets duplicate pins
  locally only because they don't link the board component.)
- **USB mode toggle is skyrizz-e32 only**; requires `rm -rf build` clean rebuild after toggling.
- **Logging**: full-runtime targets use `rt.log()`; the only sanctioned raw-stdio is the dev-board
  boot banner (before `initCore`) and the two standalone bring-up targets.
- **Boot order matters** — the keymap must be installed before the button expander posts events;
  on dev-board the e-ink panel must start before its async wrapper's display task.

## Key files

| File | Role |
|---|---|
| `firmware/core/include/nema/platform.h` / `board.h` / `system/board_profile.h` | Contracts |
| `firmware/platforms/esp32/{include/nema/esp32,src}/esp32_platform.*` | ESP32 platform |
| `firmware/platforms/esp32/.../nvs_config_store.h`, `sd_fat_filesystem.h`, `esp32_usb_{hid,cdc}.h` | ESP32 services |
| `firmware/platforms/wasm/{include/nema/wasm,src}/wasm_platform.*` | WASM platform |
| `firmware/boards/skyrizz-e32/src/skyrizz_e32.cpp`, `include/nema/skyrizze32/board_config.h`, `src/e32_key_map.cpp` | **Active board** (+ pin map) |
| `firmware/boards/dev-board/src/dev_board.cpp`, `include/nema/devboard/board_config.h` | Dev board |
| `firmware/boards/simulator/src/simulator_board.cpp` | Simulator board |
| `firmware/targets/*/main/main.cpp` + `CMakeLists.txt` + `sdkconfig.defaults` | Targets (USB toggle in skyrizz-e32) |
| `firmware/tools/build-*.sh`, `flash-*.sh` + `package.json` | Build/flash entry points |
