# 01 — Repo & Build Foundation

> Fondasi: struktur folder `firmware/`, build C++ host pakai CMake, dan wiring ke bun workspace yang sudah ada. Belum ada logika runtime — cuma "kerangka kosong yang bisa di-build & dijalankan".

- Status: ☐ Not started
- Milestone: M1 (Core Foundation) — prasyarat
- Depends on: —
- Blocks: semua stage berikutnya

---

## Goal

Setelah stage ini selesai:

- Ada folder `firmware/` lengkap sesuai layout di `00-overview.md §3`.
- `cmake` bisa meng-configure & build executable host `kairo-sim` yang isinya cuma cetak satu log "Hello Kairo" via stdout lalu exit 0.
- `bun` workspace tetap sehat (`bun install` jalan), dan ada placeholder script untuk build firmware.

## Scope

### In scope

- Layout direktori `firmware/{core,platforms,boards,targets,tools,vendor}`.
- Top-level `firmware/CMakeLists.txt` + per-modul `CMakeLists.txt`.
- Vendoring `nlohmann/json.hpp` (single header).
- `kairo-sim` executable kosong (stub `main()`).
- `.gitignore` untuk artifact build (`firmware/build/`).
- Script bantu di `firmware/tools/` dan/atau root `package.json`.

### Out of scope (ditunda)

- Build untuk ESP32 / cross-compile (target esp32 belum dibuat).
- Logika runtime apa pun (stage 02+).
- React/Bun relay (stage 10).

---

## Design

### Direktori & file baru

```text
firmware/
├─ CMakeLists.txt                  # top-level, project(kairo CXX), C++17
├─ core/
│  ├─ include/kairo/.gitkeep
│  ├─ src/.gitkeep
│  └─ CMakeLists.txt               # add_library(kairo_core ...) — sementara INTERFACE/empty
├─ platforms/simulator/CMakeLists.txt
├─ boards/simulator/CMakeLists.txt
├─ targets/simulator/
│  ├─ main.cpp                     # stub: tulis 1 baris ke stdout, return 0
│  └─ CMakeLists.txt               # add_executable(kairo-sim main.cpp) link kairo_core
├─ vendor/nlohmann/json.hpp        # single-header (download / vendor)
└─ tools/
   ├─ build-sim.sh                 # cmake configure+build
   └─ run-sim.sh                   # jalankan ./build/.../kairo-sim
```

### Top-level `firmware/CMakeLists.txt` (sketsa)

```cmake
cmake_minimum_required(VERSION 3.20)
project(kairo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # untuk clangd

add_compile_options(-Wall -Wextra)

# header-only vendor (nlohmann/json)
add_library(kairo_vendor INTERFACE)
target_include_directories(kairo_vendor INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

add_subdirectory(core)
add_subdirectory(platforms/simulator)
add_subdirectory(boards/simulator)
add_subdirectory(targets/simulator)
```

### `targets/simulator/main.cpp` (stub MVP stage ini)

```cpp
#include <cstdio>
int main() {
    std::fputs("Hello Kairo (build foundation OK)\n", stdout);
    return 0;
}
```

> Catatan: nanti di stage 02 `main.cpp` dirombak jadi boot flow asli. Sekarang cukup stub.

### Bun workspace

`packages/simulator` sudah ada (jangan diubah dulu). Tambahkan **script bantu di root `package.json`** agar build firmware bisa dipanggil dari root:

```jsonc
{
  "scripts": {
    "build:firmware": "bash firmware/tools/build-sim.sh",
    "run:firmware": "bash firmware/tools/run-sim.sh"
  }
}
```

### `firmware/tools/build-sim.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build -G "Unix Makefiles"
cmake --build build --target kairo-sim -j
```

### `.gitignore` (tambahan)

```gitignore
firmware/build/
```

---

## Tasks

- [ ] Buat struktur folder `firmware/` + `.gitkeep` pada folder kosong.
- [ ] Vendor `firmware/vendor/nlohmann/json.hpp` (single-header release).
- [ ] Tulis top-level `firmware/CMakeLists.txt` + per-modul `CMakeLists.txt` (core/platform/board sementara library kosong).
- [ ] Tulis `targets/simulator/main.cpp` stub + `CMakeLists.txt` (executable `kairo-sim`).
- [ ] `firmware/tools/build-sim.sh` & `run-sim.sh` (chmod +x).
- [ ] Update root `package.json` scripts + `.gitignore`.
- [ ] Verifikasi build & run.

## Acceptance criteria

- `bash firmware/tools/build-sim.sh` sukses tanpa error/warning fatal.
- `bash firmware/tools/run-sim.sh` mencetak `Hello Kairo (build foundation OK)` dan exit code 0.
- `bun install` di root tetap sukses; `firmware/build/` ter-ignore git.
- `compile_commands.json` tergenerate (clangd happy).

## How to verify

```bash
# dari root repo
bun run build:firmware
bun run run:firmware    # → "Hello Kairo (build foundation OK)"
echo $?                 # → 0
```

## Risks / notes

- Pastikan `cmake` & toolchain clang tersedia (macOS: `xcode-select --install`, `brew install cmake`). Jika belum, dokumentasikan di README.
- `nlohmann/json.hpp` ukurannya besar (~single header ~900KB). OK untuk host build; tidak dipakai di `core/` (core tetap agnostic) — hanya di platform/target.
