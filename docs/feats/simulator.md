# WASM Simulator

> The full firmware compiled to WebAssembly and run inside the browser — same `core/` code as
> hardware, no device required. Plans 10 / 36.

## Overview

The `wasm` target compiles `firmware/core/` + a `WasmPlatform` + the `SimulatorBoard` to
`palanu.js` + `palanu.wasm`. Palanu Forge loads it as an in-process **virtual cable** and speaks
PLP to it exactly as it would to real hardware — so the simulator screen, CLI, file browser, OTA
(dry-run) and app install all work identically.

## How it works

```
Forge /simulator                          WASM firmware (palanu.wasm)
  VirtualCableTransport.boot()             WasmPlatform + SimulatorBoard
  load /fw/palanu.js (classic Emscripten)  NullDisplay + RemoteScreenTap (screen streamed)
  send() → _nema_plp_recv(ptr,len) ──────▶ LinkService (Device)
  ◀────── Module.nemaPlpOut(frame) ─────── RemoteService (PLP over WasmCableTransport)
```

- "No glass, no stdio": the WASM device is purely a PLP endpoint; all rendering is streamed over the
  cable. Differences from ESP32: single cable (no USB+BLE mux), `WasmConfig` instead of NVS,
  `SimWifiDriver` virtual router, `SimOtaUpdater` dry-run, two in-RAM `MemFileSystem`s (`/`, `/sd`).
- **Cross-origin isolation is required** (Emscripten pthreads need SharedArrayBuffer): Forge serves
  the WASM through `/fw/[file]` with COOP/COEP/CORP headers; see
  [`../architecture/forge-web.md`](../architecture/forge-web.md).
- **Single-load**: the firmware loads once per page lifetime — Restart = full page reload with an
  `nema:autoboot` flag; Shutdown = reload to a clean off state.

## File reference

| File | Purpose |
|---|---|
| `firmware/targets/wasm/main.cpp` + `CMakeLists.txt` | WASM target + emscripten link opts |
| `firmware/platforms/wasm/src/wasm_platform.cpp` | WASM platform (cable, NullDisplay, SimWifi, SimOta, MemFS) |
| `firmware/platforms/wasm/src/wasm_cable_transport.cpp` | Virtual cable (device side) |
| `firmware/boards/simulator/src/simulator_board.cpp` | Minimal simulator board |
| `packages/forge/src/lib/transport/VirtualCableTransport.ts` | Virtual cable (host side) |
| `packages/forge/src/lib/simStore.svelte.ts` | Reactive store over the WASM session |
| `packages/forge/src/routes/simulator/+page.svelte` | The simulator UI |

## Usage

```bash
# Build the WASM firmware (needs emsdk) and run Forge:
source ~/emsdk/emsdk_env.sh
bun run forge:wasm           # = build:wasm (→ packages/forge/static/wasm/) + run Forge
```

Then open Forge → **Simulator**. Device starts **off**; click **Boot**. Use the virtual board,
CLI terminal, Settings (WiFi virtual router / display themes / inject-event), Files, Firmware
(OTA dry-run), and Logs/Events/Services panels.

## Extending

- The simulator exercises the real `core/`; most features work in the sim before hardware. Add
  sim-only behavior by extending `WasmPlatform` drivers (`SimWifiDriver`, `SimOtaUpdater`, etc.).
- `build:wasm` emits into `packages/forge/static/wasm/`; the `/fw/[file]` route serves it with the
  required headers.
