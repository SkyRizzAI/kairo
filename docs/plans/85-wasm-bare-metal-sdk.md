# Plan 85 — WASM Bare-Metal SDK (AkiraOS style)

## Goals

Switch WASM app compilation dari `wasm32-wasi` (150 KB output, butuh WASI libc) ke
**bare-metal `wasm32-unknown-unknown`** (2–5 KB output, hanya host imports).

Pendekatan ini sama dengan AkiraOS: satu header `nema_api.h` berisi semua deklarasi
host import, app C code tidak boleh `#include <stdio.h>` atau stdlib — semua I/O lewat
`nema_*` functions yang disediakan firmware di runtime.

---

## Background

Plan 84 Fase 4a berhasil: WASM headless jalan via wasm3. Tapi compile dengan
`--target=wasm32-wasi` menyertakan seluruh WASI libc (~140 KB), membuat app yang
logikanya cuma ~1 KB jadi 150 KB. Selain boros, WASI libc juga menyembunyikan
dependency sebenarnya — app bisa `printf` ke stdout tanpa sadar itu WASI syscall.

AkiraOS pakai `-nostdlib --target=wasm32-unknown-unknown`: app hanya bisa panggil
fungsi yang eksplisit ada di header SDK. Hasilnya: ~2–5 KB per app, dependency
transparan, tidak ada WASI runtime overhead.

---

## Perubahan

### 1. `packages/app-sdk/include/nema_api.h` (BARU)

Single header, semua nema host imports. Dua attribute penting:
- `NEMA_IMPORT(mod, name)` → `__attribute__((import_module(mod), import_name(name)))` untuk declare host functions
- `NEMA_EXPORT` → `__attribute__((export_name("main")))` yang WAJIB di `main()`.
  Tanpa ini, lld DCE menghapus main karena tidak ada caller intra-module.

Fungsi yang di-expose oleh `wasm_nema.cpp`:
- `nema_log(level, tag, msg)` → void
- `nema_print(msg)` → void (menggantikan `printf`, route ke rt.log stdout sink)
- `nema_device_name(out, cap)` → int
- `nema_device_caps(out, cap)` → int (newline-separated)
- `nema_storage_fs_read_file(name, out, cap)` → int
- `nema_storage_fs_write_file(name, data, len)` → int

Minimal libc inline (no WASI): `nema_strlen`, `nema_itoa`, `nema_atoi`,
`nema_memcpy`, `nema_memset`.

### 2. `packages/app-sdk/bin/build.ts`

Compile flags WASM:
```
--target=wasm32-unknown-unknown -nostdlib -O2 -fvisibility=hidden
-Wl,--no-entry -Wl,--allow-undefined -Wl,--strip-all
-I <nema_api.h dir>
```

Catatan: `-Wl,--export=main` TIDAK diperlukan — `NEMA_EXPORT` di source lebih reliable
(linker flag tidak cukup; butuh `export_name` attribute di compiler level).

### 3. `firmware/core/src/wasm/wasm_nema.cpp`

- Tambah `nema_print(msg)` host function → route ke rt.log info
- Register di `linkNemaImports`

### 4. `firmware/core/src/wasm/wasm_engine.cpp`

`runStart()` sekarang detect tipe app:
- **Bare-metal** (ada `main` export): panggil `main(argc=0, argv=NULL)` langsung
- **WASI legacy** (ada `_start`): panggil `_start()` — backward compat

### 5. Rewrite 3 WASM examples

`examples/counter-wasm/main.c`, `hello-wasm/main.c`, `sysinfo-wasm/main.c`:
- Hapus `#include <stdio.h>`, `#include <string.h>`
- Ganti `printf(...)` → `nema_print(...)` atau `nema_log(...)`
- Main function ditandai `NEMA_EXPORT int main(...)`
- Hanya `#include "nema_api.h"`

---

## Checklist

- [x] `packages/app-sdk/include/nema_api.h` — single header semua nema imports + `NEMA_EXPORT`
- [x] `build.ts` — compile flags bare-metal (`wasm32-unknown-unknown -nostdlib -fvisibility=hidden`)
- [x] `wasm_nema.cpp` — tambah `nema_print`
- [x] `wasm_engine.cpp` — detect bare-metal vs WASI; call `main(0,0)` atau `_start()`
- [x] `examples/counter-wasm/main.c` — rewrite tanpa stdio, `NEMA_EXPORT int main`
- [x] `examples/hello-wasm/main.c` — rewrite tanpa stdio, `NEMA_EXPORT int main`
- [x] `examples/sysinfo-wasm/main.c` — rewrite tanpa stdio, `NEMA_EXPORT int main`
- [x] 13/13 host tests pass setelah perubahan `wasm_engine.cpp`
- [x] Test build: hello=620B, counter=1.6KB, sysinfo=784B (via Docker wasi-sdk-25)
- [ ] Test: upload counter-wasm.papp ke device, jalan dari launcher
- [ ] Test: counter storage (inc/dec) persist antar launch

---

## File yang terdampak

| File | Perubahan |
|------|-----------|
| `packages/app-sdk/include/nema_api.h` | BARU |
| `packages/app-sdk/bin/build.ts` | Update WASM compile flags + `-I` path |
| `firmware/core/src/wasm/wasm_nema.cpp` | Tambah `nema_print` |
| `firmware/core/src/wasm/wasm_engine.cpp` | Bare-metal `main` detection |
| `examples/counter-wasm/main.c` | Rewrite bare-metal |
| `examples/hello-wasm/main.c` | Rewrite bare-metal |
| `examples/sysinfo-wasm/main.c` | Rewrite bare-metal |

---

## Status

DONE — semua host-side code sudah diimplementasi dan test pass. Tinggal device test.
