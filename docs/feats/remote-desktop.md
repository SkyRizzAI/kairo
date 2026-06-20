# Forge Remote Desktop

> Drive the device's screen, input, logs, shell and filesystem from a browser (Palanu Forge)
> over the PLP protocol — across USB-Serial, Bluetooth, or the in-browser WASM cable.

## Overview

The device exposes a transport-agnostic "console substrate": one wire protocol (PLP) carries the
screen mirror, key injection, logs, events, CLI and file operations. **Palanu Forge** (the
SvelteKit host app) connects over any of three transports and renders a live device using the
board's own profile. The exact same `RemoteSession` + `SessionView` UI works for all three.

## Transports

| Transport | Host side | Device side | Notes |
|---|---|---|---|
| **USB-Serial** | Web Serial (`navigator.serial`, 921600) | native USB Serial/JTAG (HWCDC) or USB-CDC | See [ADR 0001](../decisions/0001-usb-jtag-remote-uses-hwcdc.md): in JTAG mode the device must drive HWCDC, not UART0. |
| **Bluetooth** | Web Bluetooth GATT | `Esp32Ble` (NimBLE) | GATT service/TX/RX UUIDs must match `plp/uuids.ts`; mtu 180. |
| **WASM cable** | in-process virtual cable | WASM firmware | The simulator — no hardware. See [`simulator.md`](simulator.md). |

## How it works

```
Browser (Forge)                     Device
  RemoteSession                       RemoteService + LinkService
  ───────────────                     ────────────────────────────
  connect transport ───────────────▶ (USB/BLE/WASM)
  send Control HELLO (retry 300ms) ─▶ LinkService: ready_=true, reply ACK
  on ACK: send System GetInfo ──────▶ reply board-profile JSON
  ◀───────────── Screen frames (RLE 1-bit), Log, Event, Cli, File, Ota replies
  sendKey / sendCli / file ops ─────▶ dispatched per PLP channel
```

PLP frame: `[0xAB][chan][flags][len:2 LE][payload][crc8]`, CRC-8/SMBus. Channels: Control, Screen,
Input, Log, System, Ota, Ext, Event, Cli, File. The handshake is HELLO→ACK; app channels are gated
until ready. Full protocol in [`../architecture/link-remote-storage.md`](../architecture/link-remote-storage.md).

## File reference

| Side | File |
|---|---|
| Device — orchestrator | `firmware/core/src/services/remote_service.cpp` |
| Device — session/handshake | `firmware/core/src/link/link_service.cpp` |
| Device — transports | `firmware/core/include/nema/link/{mux,usb_cdc_link,ble_link}_transport.h` |
| Device — codec | `firmware/core/src/link/plp_codec.cpp` |
| Host — client | `packages/forge/src/lib/RemoteSession.ts` |
| Host — transports | `packages/forge/src/lib/transport/{Serial,Ble,VirtualCable}Transport.ts` |
| Host — UI | `packages/forge/src/lib/components/SessionView.svelte`, `BoardVisual.svelte` |

## Usage

1. Open Forge → **Remote**. Pick Simulator, Bluetooth, or USB.
2. Grant the browser device permission (Web Serial/Bluetooth prompts; Chromium-family only).
3. The device appears as a live `BoardVisual` (screen mirror + clickable buttons), with CLI, file
   browser, and firmware (OTA) panels.

## Gotchas

- Opening a USB-Serial port resets the chip (auto-reset); the first ~3-4 s is boot — Forge's 300 ms
  HELLO retry reconnects automatically once the device is ready.
- Forge is client-only and needs a Chromium-family browser for Web Serial/Bluetooth.
- The PLP codec and BLE UUIDs are a cross-repo contract — keep firmware and `packages/forge` in sync.

## Extending

- New transport: implement `ILinkTransport` on both sides (device C++ + `packages/forge` TS) and add
  it to the `MuxTransport` / the Forge discovery picker.
- New channel/op: extend the `Channel`/opcode enums in BOTH `plp_codec.h`/`remote_service.cpp` and
  the TS `codec.ts`/`RemoteSession.ts`.
