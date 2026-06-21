# Examples

Kumpulan contoh app untuk Palanu. Tiap folder adalah source `.papp` — tinggal build & install.

Setiap fitur punya dua versi: **JS** (TSX, jalan sekarang) dan **WASM** (C, toolchain Plan 84 Fase 4).

## Daftar contoh

| Folder | Runtime | Mode | Deskripsi |
|--------|---------|------|-----------|
| `counter/` | JS | UI | Counter dengan persist storage via `nema.storage.fs` |
| `counter-wasm/` | WASM | CLI | Counter versi C — inc/dec/reset via args |
| `sysinfo/` | JS | UI | Device name + capabilities list + tap counter |
| `sysinfo-wasm/` | WASM | CLI | Sysinfo versi C — print device info ke stdout |
| `hello/` | JS | UI | Minimal hello + counter |
| `hello-papp/` | JS | Hybrid | Hello dengan mode CLI + UI |
| `hello-wasm/` | WASM | CLI | Hello versi C — greet via args |
| `deauth/` | JS | UI | WiFi tools demo |

> **WASM examples**: source C sudah ada, tapi butuh wasi-sdk toolchain.
> Toolchain di-setup di Plan 84 Fase 4.

## Build (JS apps)

```bash
# Dari folder example:
cd examples/counter
bun run ../../packages/app-sdk/bin/build.ts .

# Output: dist/com.palanu.example.counter.papp/
```

## Build (WASM apps — setelah Plan 84 Fase 4)

```bash
cd examples/counter-wasm
clang --target=wasm32-wasi -o counter.wasm main.c
# Lalu bundle ke .papp dengan app-sdk
```

## Install ke device

Copy folder `dist/<id>.papp/` ke `/system/apps/` di device via Forge FileBrowser atau CLI.

## Icon

Simpan `icon.png` (32×32) di folder source. SDK otomatis convert ke `icon.raw` (RGB565) saat build.
