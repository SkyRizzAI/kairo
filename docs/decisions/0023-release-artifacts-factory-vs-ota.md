# 0023 — Release artifacts: factory vs OTA images

- **Status:** accepted
- **Date:** 2026-07-01

## Context

A board maker flashed a fresh SkyRizz Solana board with the firmware from our GitHub
Release and it boot-looped:

```
load:0x42000020,len:0x1fb8f4
Invalid image block, can't boot.   (ets_main.c 329)
```

Root cause: the release attached **only the app image** (`palanu-<board>.bin`). That
file is the ESP-IDF *application* partition image — not a bootable flash image. When
written to offset `0x0` (the natural thing to do with a lone `.bin`), the ROM reads
the app's segments (`0x3c200020`, `0x42000020`) as if they were a bootloader and
rejects them. The app-only image is correct **only** for OTA (it goes to the A/B app
slot; the bootloader/partition table already exist on the device).

Investigating surfaced two more latent defects:

1. `firmware/tools/publish-firmware.sh` (feeds Forge's `/flash` Web-Serial flasher)
   still looked for `nema-<t>.bin` after the rename to `palanu-*`, so the registry
   was empty; and it hard-coded the app at `0x10000` while `partitions.csv` puts the
   A/B app at `0x20000` — flashing via that path would itself brick.
2. Forge's OTA picker matched **any** `palanu-*.bin`, so a full-flash image would
   appear as an OTA option and, if chosen, overflow the ~5 MB slot.

The three flashing paths need different files, and nothing made that explicit:
cable-to-`0x0` (full image), OTA (app-only), and `/flash` Web-Serial (multi-part).

## Decision

1. **Two clearly-named artifacts per board, per release** — the filename states the use:
   - `palanu-<board>-<ver>-factory.bin` — full merged image (bootloader + partition
     table + otadata + app + spiffs), flashed to `0x0`. Blank board / recovery.
   - `palanu-<board>-<ver>-ota.bin` — app-only image for OTA (and app-slot reflash).
   Each with a `.sha256` sidecar. A `FLASHING.md` guide is attached to the release.

2. **One shared packager** `firmware/tools/package-firmware.sh <board>` builds and
   emits both, deriving the merge offsets from the build's `flasher_args.json` (never
   hard-coded). CI (`ci.yml`) and the release workflow (`release-please.yml`) both
   call it via a **board matrix**, so adding a board is a one-line matrix edit.

3. **Forge OTA picker only lists `-ota.bin`** (plus `.wasm` for the simulator), and
   the panel refuses any image larger than the OTA slot (`0x500000`) as a safety net —
   a factory image can never be streamed to the A/B slot.

4. **Fix `publish-firmware.sh`** to name `palanu-*`, derive all parts + offsets +
   flash params from `flasher_args.json`, and stage every part (incl. otadata +
   spiffs) so the `/flash` Web-Serial path produces a fully working device.

## Consequences

- A supplier can flash a blank board with a single file (`-factory.bin` → `0x0`) or
  the browser `/flash` page, and OTA users get `-ota.bin` — no offset knowledge, no
  wrong-file footgun.
- Offsets live in exactly one place (the build's `flasher_args.json`); the class of
  "hard-coded offset drifted from partitions.csv" bug is gone.
- Old releases (pre-this-change) exposed `palanu-<board>.bin`; after the picker change
  those no longer appear as OTA options. Going forward every release uses the
  `-factory`/`-ota` scheme. See ADR [0022](0022-skyrizz-solana-board-bringup.md) for
  the board itself, and [`docs/FLASHING.md`](../FLASHING.md) for the user guide.
- Trade-off: two ESP-IDF build legs per release (matrix) instead of one shared build;
  acceptable for isolation and simplicity. Factory images are ~10 MB each.
