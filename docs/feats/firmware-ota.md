# Firmware OTA Updates

> Plan 39 — Update the device firmware over the wire (PLP Ota channel) with A/B partitions and
> automatic rollback. No secure boot.

## Overview

Forge streams a new firmware image to the device over the PLP **Ota** channel; the device writes it
to the inactive A/B slot and reboots into it. If the new image fails to confirm a good boot, the
ESP32 bootloader rolls back to the previous slot. Backed by `IOtaUpdater` (`esp_ota_*` on ESP32; a
dry-run on the simulator).

## How it works

```
Forge RemoteSession.otaUpdate(bin)        Device RemoteService.handleOta + IOtaUpdater
  Begin [size:4] ──────────────────────▶  open inactive slot; reply [written:4][protoVersion=2]
  Data [offset:4][bytes] × N ──────────▶  idempotent by offset (off==written→write;
   (1792 B chunks, 8× retry)                off<written→ack; off>written→error→host resyncs)
  End ─────────────────────────────────▶  commit; if rebootOnCommit → SysOp::Restart
  (next boot) ─────────────────────────▶  confirmBoot() cancels pending rollback
```

- **Idempotent by offset** — dropped frames never double-write; the host resyncs from `written()`.
- **Protocol version 2** in the Begin reply lets the host detect stale firmware.
- **A/B + rollback** — the running image must call `confirmBoot()` (done during `registerServices`);
  if it never does (crash loop), the bootloader reverts.

## File reference

| Side | File |
|---|---|
| Device — channel handler | `firmware/core/src/services/remote_service.cpp` (`handleOta`) |
| Device — interface | `firmware/core/include/nema/hal/ota.h` (`IOtaUpdater`) |
| Device — ESP32 impl | `firmware/platforms/esp32/src/esp32_ota.cpp` (`esp_ota_*`, rollback) |
| Device — sim impl | WASM `SimOtaUpdater` (dry-run, `rebootOnCommit()=false`) |
| Host — client | `packages/forge/src/lib/RemoteSession.ts` (`otaUpdate`, `OTA_PROTO=2`) |
| Host — UI | `packages/forge/src/lib/components/FirmwarePanel.svelte` |
| Host — registry | `packages/forge/src/lib/trpc/router.ts`, `static/firmware/manifest.json` |
| Build publish | `firmware/tools/publish-firmware.sh` |

## Usage

1. Publish a build: `firmware/tools/publish-firmware.sh` writes `static/firmware/manifest.json`
   (the tRPC `firmware.list` reads it).
2. In Forge, connect to the device (Remote) → **Firmware** panel → pick a build → update.
3. OTA `.bin` downloads from GitHub releases go through `/api/firmware-proxy` (the cross-origin
   isolated page can't fetch them directly). The Firmware panel lists **only** `-ota.bin`
   assets and refuses any image larger than the A/B slot (`0x500000`) — a full-flash
   `-factory.bin` can never be pushed over OTA (it would overflow the slot). See ADR 0023.

## Release artifacts (which file for what)

Each release ships two clearly-named images per board (ADR 0023, [`FLASHING.md`](../FLASHING.md)):

- `palanu-<board>-<ver>-**factory**.bin` — full image → **cable-flash to `0x0`** (blank board / recover).
- `palanu-<board>-<ver>-**ota**.bin` — app-only → **OTA** (this feature) or app-slot reflash.

Both are produced by `firmware/tools/package-firmware.sh <board>`; offsets come from the
build's `flasher_args.json`, never hard-coded.

## Related: client-side flashing

A full erase/flash (not OTA) is available at Forge **/flash** via esptool-js over Web Serial.
It flashes every part at the offsets from the firmware manifest (bootloader `0x0`, partition
table `0x8000`, app `0x20000`, otadata, spiffs) — staged by `publish-firmware.sh`. Use this for
the first flash or to recover; OTA is for in-field updates.

## Gotchas

- **OTA chunk = 1792 B** — a tuned value; 4096 overflowed the USB-CDC RX ring.
- Partition layout is `partitions.csv` (A/B, ~5 MB slots) set via `sdkconfig.defaults`.

## Extending

- The header notes an HTTP/WiFi *pull* source is possible — it would feed the same
  `begin/write/commit` sink in `IOtaUpdater`. Only the PLP push path exists today.
