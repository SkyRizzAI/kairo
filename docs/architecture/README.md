# Palanu — Architecture Reference

> Generated from a full codebase scan on **2026-06-20**. This is the detailed
> reference companion to [`../overview.md`](../overview.md) (narrative snapshot) and
> [`../STATE.md`](../STATE.md) (status). When code and these docs disagree, the code
> wins — fix the doc (see CLAUDE.md "Documentation — automated upkeep").

Palanu is a hardware-agnostic embedded firmware runtime (C++17). **One core, many
boards.** The same `firmware/core/` compiles to ESP32-S3 hardware and to a WASM
browser simulator; only the platform/board layers change.

## The four layers

```
┌──────────────────────────────────────────────────────────────────────┐
│  TARGET   firmware/targets/*  — buildable app: one main.cpp that wires │
│           a platform + a board + a Runtime, then runs the loop.        │
├──────────────────────────────────────────────────────────────────────┤
│  BOARD    firmware/boards/*   — IBoard: physical hardware wired to the │
│           chip (display, buttons, sensors), pin map, keymap, profile.  │
├──────────────────────────────────────────────────────────────────────┤
│  PLATFORM firmware/platforms/* — IPlatform: chip/OS environment        │
│           (clock, WiFi, BLE, USB, NVS, filesystem, OTA, power).        │
├──────────────────────────────────────────────────────────────────────┤
│  CORE     firmware/core/      — hardware-free runtime: boot, services, │
│           Nema kernel (threads), UI, app model, link/PLP, VFS, HAL     │
│           interfaces, scripting. No hardware dependencies.             │
└──────────────────────────────────────────────────────────────────────┘
```

The boot sequence is identical for every target:
`loadPlatform → loadBoard → initCore → registerServices → start → push HomeScreen → step()`.

## Documents

| Doc | Covers |
|---|---|
| [`runtime-kernel.md`](runtime-kernel.md) | Boot flow, `Runtime`, service container/manager, Nema kernel (Thread / MessageQueue / TaskRunner), capability & resource model, event bus, logger, config store, process model, threading. |
| [`ui-app-input.md`](ui-app-input.md) | Canvas (1-bit), ViewDispatcher, component/flex UI, app model (`IApp`/`AppHost`), input abstraction (Action/Code/Key + gestures), assets/fonts/animation, screens & built-in apps. |
| [`ui-design-system.md`](ui-design-system.md) | **Design-oriented** consolidation for a production UI concept: physical 1-bit constraints, `StyleTokens` design tokens, typography/`TextRole`, flex layout rules, the 1-bit visual language, component vocabulary, full screen map, app-facing UI surface, design invariants & open questions. |
| [`link-remote-storage.md`](link-remote-storage.md) | PLP wire protocol, transports (USB-CDC / BLE / WASM cable / mux), `LinkService`, `RemoteService` channels, CLI, OTA, VFS & filesystem backends, HAL interface map. |
| [`scripting-and-apps.md`](scripting-and-apps.md) | QuickJS (UI apps) + wasm3 (headless) runtimes, sandboxing, the app store, `.papp` packaging, embedded apps. |
| [`platforms-boards-targets.md`](platforms-boards-targets.md) | `IPlatform`/`IBoard` contracts, ESP32 & WASM platforms, the boards (skyrizz-e32, dev-board, simulator), all targets, build/flash, USB mode toggle. |
| [`forge-web.md`](forge-web.md) | Palanu Forge (SvelteKit host app): browser transports, PLP-in-TS, routes, flasher, the app-sdk, the IDL, and the firmware↔host cross-repo contracts. |

## Cross-repo contracts (must stay byte-for-byte in sync)

These pairs are the same protocol/interface implemented twice; changing one without the
other breaks the device↔host link silently:

| Contract | Firmware side | Host side |
|---|---|---|
| PLP wire codec (magic `0xAB`, CRC-8/0x07, 10 channels) | `firmware/core/.../link/plp_codec.*` | `packages/forge/src/lib/plp/codec.ts` |
| BLE GATT UUIDs | `firmware/core/.../link/plp_ble.h` | `packages/forge/src/lib/plp/uuids.ts` |
| Board profile JSON | `RemoteService` / `BoardProfile` | `RemoteSession.ts` / `BoardVisual.svelte` |
| Nema System API | `generated/host/*` (from IDL) | `packages/app-sdk` `nema.d.ts` (from IDL) |
| App package formats | embedded-app / OTA loader | `.papp` from `app-sdk` |

Single source of truth for the System API is the IDL (`api/*.pidl` → `packages/idl`).

## Related decisions

- [ADR 0001](../decisions/0001-usb-jtag-remote-uses-hwcdc.md) — Forge remote over USB
  Serial/JTAG drives HWCDC directly (not Arduino `Serial`).
