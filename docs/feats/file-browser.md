# File Browser & Storage

> Plan 38 — A Linux-style virtual filesystem (VFS) that unifies internal flash, RAM scratch and
> microSD behind one tree, browsable on-device and from Forge.

## Overview

All storage is presented as one namespace rooted at `/` via a `Vfs` composite. The on-device
`FileBrowserScreen` and the Forge `FileBrowser` panel both operate on this tree (the latter over the
PLP File channel), so they show the same files regardless of backend.

## Mount table

| Mount | Backend | Persistence |
|---|---|---|
| `/` | LittleFS (internal flash `spiffs` partition) on ESP32; RAM on WASM | persistent (ESP32) |
| `/tmp` | `MemFileSystem` (RAM) | volatile; mounts even if `/` fails |
| `/sd` | FAT (`SdFatFileSystem`) | only when a microSD card is present (SkyRizz E32) |

The VFS routes a path to the **longest** matching mount and strips the prefix; mount points appear
as synthetic directories in `list()` (so `ls /` shows `sd`/`tmp`). Seeded on first boot: `/apps`,
`/data`, `/badusb` (+ factory Ducky scripts), `/readme.txt`.

## How it works

```
On-device: FileBrowserScreen ──▶ IFileSystem (Vfs) ──▶ backend (LittleFS/Mem/SdFat)
Forge:     FileBrowser ── PLP File channel ──▶ RemoteService.handleFile ──▶ Vfs
```

`IFileSystem` ops: `list / read / write / mkdir / remove / rename` (v1 is whole-file I/O — no
chunked offsets yet). The File channel mirrors these as `[op][status][path\0][extra]`.

## File reference

| File | Purpose |
|---|---|
| `firmware/core/include/nema/hal/filesystem.h` | `IFileSystem` interface, `FsEntry` |
| `firmware/core/src/fs/vfs.cpp` | VFS composite (mount table, longest-match routing) |
| `firmware/core/src/fs/mem_filesystem.cpp` | RAM backend |
| `firmware/platforms/esp32/src/littlefs_filesystem.cpp` | internal-flash backend |
| `firmware/platforms/esp32/src/sd_fat_filesystem.cpp` | microSD FAT backend |
| `firmware/core/src/screens/file_browser_screen.cpp` | on-device browser (folders-first, descend/up) |
| `firmware/core/src/services/remote_service.cpp` | `handleFile` (PLP File channel) |
| `packages/forge/src/lib/components/FileBrowser.svelte` | host browser UI |

## Usage

- **On device**: HomeScreen carousel → **Files**. Activate descends into folders; Back goes up / to
  Home at root.
- **From Forge**: connect → **Files** panel: list/read/write/mkdir/remove/rename/copy.
- **microSD** (SkyRizz E32): insert a FAT card; `/sd` auto-mounts and surfaces in the tree (pins in
  `board_config.h`, see [`../architecture/platforms-boards-targets.md`](../architecture/platforms-boards-targets.md)).

## Gotchas

- Mount points cannot be `mkdir`/`remove`/`rename`d; cross-backend `rename` is unsupported.
- `/tmp` is volatile; persistent data belongs under `/` or `/sd`.

## Extending

- New backend: implement `IFileSystem` and `vfs_.mount("/point", &backend)` in the platform.
- Chunked/offset I/O is a known v1 gap (whole-file only today).
