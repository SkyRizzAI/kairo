# 41 — Rebrand: Palanu → PalanuOS / Nema

> Penggantian nama menyeluruh dari "Palanu" ke ekosistem baru:
> - **Palanu** — nama OS dan ekosistem (brand utama, user-facing)
> - **Nema** — nama core kernel (sudah direncanakan sejak Plan 19.5)
> - **Forge** — nama web tool, tidak berubah (sudah benar)
>
> Total occurrences yang perlu diubah: ~83.676 di seluruh codebase.
> Breaking changes sesungguhnya: **sangat sedikit** (lihat Fase 5).

- Status: 📝 **PLANNED** (2026-06-11)
- Milestone: M-Rebrand

---

## 0. Keputusan Kunci

### 0.1 Pemetaan nama

| Konteks | Lama | Baru | Catatan |
|---|---|---|---|
| C++ namespace | `nema::` | `nema::` | Nema = kernel; semua firmware code |
| Header path | `#include "nema/..."` | `#include "nema/..."` | Sesuai folder rename |
| Directory | `include/palanu/` | `include/nema/` | Semua platform |
| CMake targets | `nema_core`, `nema_platform_*` | `nema_core`, `nema_platform_*` | |
| Package root | `name: "palanu"` | `name: "palanu"` | workspace root |
| App SDK package | `"palanu"` (import) | `"nema"` | Apps: `import { View } from "nema"` |
| App IDs | `com.palanu.*` | `com.palanu.*` | App registry IDs |
| BLE default name | `"Palanu"` | `"Palanu"` | User-visible device name |
| Default user | `"Palaner"` | `"Palanut"` ⚠️ | **Keputusan user: mau diganti jadi apa?** |
| Default device | `"My Palanu"` | `"My Palanu"` | NVS default, first-boot only |
| Protocol name | `KLP (Nema Link Protocol)` | `NLP (Nema Link Protocol)` | Nama saja, format binary tidak berubah |
| NVS namespace wifi | `"palanu_wifi"` | `"palanu_wifi"` | ⚠️ Perlu migration (lihat 0.3) |
| WASM output | `nema.wasm`, `palanu.js` | `nema.wasm`, `nema.js` | Build script + Forge loader |
| JS folder | `/forge/static/wasm/palanu.*` | `/forge/static/wasm/nema.*` | |

### 0.2 Kenapa `nema::` bukan `palanu::`?

Nema sudah ditetapkan sebagai nama kernel sejak Plan 19.5. Firmware = kernel Nema.
`nema::` di C++ konsisten: semua kode yang ditulis di firmware adalah kode Nema,
bukan kode Palanu (Palanu adalah brand OS-nya, bukan nama API internal).

Analogi: Android OS = brand, tapi di kodenya `android::`, `com.android.*`.

### 0.3 NVS namespace "palanu_wifi" — migrasi data

NVS namespace ini menyimpan SSID + password WiFi yang disimpan persistent.
Kalau langsung diganti tanpa migrasi → user harus reconnect WiFi setelah update.

Strategi migrasi ringan (saat `init()`):
```cpp
// Cek apakah key ada di namespace lama, kalau iya migrate ke baru lalu hapus lama
std::string ssid;
if (cfg.getString("palanu_wifi", "ssid", ssid)) {
    std::string pw;
    cfg.getString("palanu_wifi", "password", pw);
    cfg.setString("palanu_wifi", "ssid", ssid);
    cfg.setString("palanu_wifi", "password", pw);
    cfg.remove("palanu_wifi", "ssid");
    cfg.remove("palanu_wifi", "password");
}
```

### 0.4 Fungsi WASM `nema_nlp_recv` / `Module.nemaKlpOut`

Ini bukan benar-benar "breaking" karena firmware dan Forge di-build bersamaan —
rename koordinasi antara `wasm_cable_transport.cpp` dan kode JS Forge cukup.
Ganti ke `nema_nlp_recv` / `Module.nemaKlpOut` (NLP = Nema Link Protocol).

---

## 1. Scope Perubahan

### Kode yang berubah paling banyak

| File | Tipe perubahan |
|---|---|
| `firmware/core/include/palanu/**/*.h` | Folder rename + namespace |
| `firmware/core/src/**/*.cpp` | namespace + includes |
| `firmware/platforms/*/include/palanu/**` | Folder rename + namespace |
| `firmware/platforms/*/src/*.cpp` | namespace + includes |
| `firmware/boards/*/include/palanu/**` | Folder rename + namespace |
| `packages/nema-app-sdk/**` | Package rename + folder |
| `packages/forge/src/**/*.{ts,svelte}` | Import paths + strings |
| `firmware/core/src/js/js_engine.cpp` | Module name `"palanu"` → `"nema"` |
| `docs/plans/*.md` | Text references |

### Yang TIDAK berubah

- Format binary KLP frame (`[0xAB][chan][flags][len][payload][crc8]`) — tidak berubah
- BLE UUID (`6E400001-B5A3-F393-E0A9-E50E24DCCA9E` dll) — tidak berubah
- NVS keys dalam namespace (bukan nama namespace-nya): `"ssid"`, `"user"`, `"device"`, dll
- Nama file build output final (boleh berubah, tapi bukan breaking di level protocol)

---

## 2. Fase Pengerjaan

### Fase 1 — Rename direktori headers (paling dulu, paling kritikal)

Semua `include/palanu/` → `include/nema/`.

```bash
# Tiap platform:
mv firmware/core/include/palanu        firmware/core/include/nema
mv firmware/platforms/esp32/include/palanu firmware/platforms/esp32/include/nema
mv firmware/platforms/wasm/include/palanu  firmware/platforms/wasm/include/nema
mv firmware/boards/dev-board/include/palanu   firmware/boards/dev-board/include/nema
mv firmware/boards/skyrizz-e32/include/palanu firmware/boards/skyrizz-e32/include/nema
mv firmware/boards/simulator/include/palanu   firmware/boards/simulator/include/nema
```

Setelah mv, semua `#include "nema/..."` akan gagal build → masuk Fase 2.

### Fase 2 — Ganti semua references teks di firmware (scripted)

Gunakan `sed` / `find+replace` massal:

```bash
# namespace
find firmware -name "*.h" -o -name "*.cpp" | \
  xargs sed -i '' 's/namespace nema/namespace nema/g; s/nema::/nema::/g'

# include paths
find firmware -name "*.h" -o -name "*.cpp" | \
  xargs sed -i '' 's|#include "nema/|#include "nema/|g'

# CMake targets
find firmware -name "CMakeLists.txt" | \
  xargs sed -i '' 's/nema_core/nema_core/g; s/nema_platform/nema_platform/g; s/nema_board/nema_board/g; s/project(nema/project(nema/g'

# ESP32 CMake REQUIRES
find firmware -name "CMakeLists.txt" | \
  xargs sed -i '' 's/REQUIRES core/REQUIRES nema_core/g'

# User-visible strings
find firmware -name "*.cpp" -o -name "*.h" | \
  xargs sed -i '' 's/"Palanu"/"Palanu"/g; s/"My Palanu"/"My Palanu"/g'

# App IDs
find firmware -name "*.h" -o -name "*.cpp" | \
  xargs sed -i '' 's/com\.palanu\./com.palanu./g'

# NVS wifi namespace
find firmware -name "*.cpp" | \
  xargs sed -i '' 's/"palanu_wifi"/"palanu_wifi"/g'

# Protocol function names (WASM bridge)
find firmware -name "*.cpp" | \
  xargs sed -i '' 's/nema_nlp_recv/nema_nlp_recv/g'

# Filesystem labels
find firmware -name "*.cpp" | \
  xargs sed -i '' 's/Palanu filesystem/Palanu filesystem/g; s/Palanu virtual filesystem/Palanu virtual filesystem/g'
```

### Fase 3 — Rename packages web

```bash
# Rename folder SDK
mv packages/nema-app-sdk packages/nema-app-sdk

# Update package.json names
# packages/nema-app-sdk/package.json: "name": "nema"
# packages/forge/package.json: update deps
# root package.json: workspaces, name
```

Ganti semua `import ... from "palanu"` → `"nema"` di:
- `packages/nema-app-sdk/**`
- `packages/forge/src/**`
- `firmware/core/src/js/js_engine.cpp` (module name check)

### Fase 4 — WASM output rename

```bash
# Di build-wasm.sh / CMakeLists wasm:
# Output: nema.wasm → nema.wasm, palanu.js → nema.js

# Di Forge static loader:
mv packages/forge/static/wasm/nema.wasm packages/forge/static/wasm/nema.wasm
mv packages/forge/static/wasm/palanu.js   packages/forge/static/wasm/nema.js

# Update Forge JS references: Module.nemaKlpOut → Module.nemaKlpOut
find packages/forge/src -name "*.ts" -o -name "*.svelte" | \
  xargs sed -i '' 's/nemaKlpOut/nemaKlpOut/g; s/palanu\.wasm/nema.wasm/g; s/palanu\.js/nema.js/g'
```

### Fase 5 — Migrasi NVS wifi (firmware)

Tambah migration call di `Esp32WifiDriver::init()` (lihat 0.3).
Hanya perlu jalan sekali pada device yang sudah pernah save WiFi.

### Fase 6 — Dokumentasi

```bash
find docs -name "*.md" | \
  xargs sed -i '' 's/Palanu/Palanu/g; s/palanu/palanu/g'

# Rename plan yang menyebut "Palanu Link":
# "KLP" → "NLP", "Nema Link Protocol" → "Nema Link Protocol"
```

### Fase 7 — Root folder rename (opsional, last)

```bash
cd ..
mv palanu palanu
# Update git remote URL kalau perlu
```

Root folder rename adalah murni kosmetik — tidak ada kode yang hardcode path absolute.

---

## 3. Build Verification Checklist

Setelah tiap fase:
- [ ] `bun run build:skyrizz-e32` — tidak ada error
- [ ] `bun run build:wasm` — tidak ada error
- [ ] `bun run dev` (Forge) — tidak ada error
- [ ] Simulator konek ke Forge — screen muncul
- [ ] Hardware flash + konek — screen muncul

---

## 4. File yang butuh perhatian manual (tidak bisa scripted)

| File | Masalah |
|---|---|
| `firmware/core/src/js/js_engine.cpp` | `strcmp(name, "palanu")` — module name check, ganti ke `"nema"` |
| `packages/forge/src/lib/transport/WasmTransport.ts` | WASM module loader, nama file `.wasm` |
| `firmware/boards/skyrizz-e32/include/palanu/skyrizze32/board_config.h` | Subfolder path + board identifiers |
| `firmware/platforms/wasm/src/wasm_cable_transport.cpp` | `EMSCRIPTEN_KEEPALIVE void nema_nlp_recv` |
| `CLAUDE.md` (project instructions) | Semua instruksi menyebut "Palanu" |
| Root `README.md` | Marketing text, semua manual |

---

## 5. Keputusan yang perlu dikonfirmasi dulu

Sebelum eksekusi, konfirmasi:

1. **Default user name**: "Palaner" → apa? (`"Palanu"` / `"User"` / biarkan?)
2. **Nama CLI/terminal prompt**: `palanu>` atau tidak ada prompt?
3. **`"palanu"` NVS namespace lain** (selain wifi): ada `"palanu_wifi"` saja yang ditemukan —
   namespace lain (`"profile"`, `"wifi"`, `"display"` dll) sudah generic, tidak perlu diubah.
4. **Root folder**: ganti `palanu/` → `palanu/` sekarang, atau nanti setelah code selesai?
5. **Git remote/repo**: nama repo di GitHub mau ikut diganti?

---

## 6. Estimasi effort

| Fase | Effort | Keterangan |
|---|---|---|
| 1 — Dir rename | 15 menit | `mv` + verify build error |
| 2 — Firmware text (scripted) | 30 menit | Sed massal + fix manual residual |
| 3 — Web packages | 30 menit | Package.json + imports |
| 4 — WASM rename | 20 menit | Build script + Forge loader |
| 5 — NVS migration | 15 menit | Satu fungsi kecil |
| 6 — Docs | 20 menit | Sed massal |
| 7 — Root folder | 5 menit | Optional |
| **Total** | **~2.5 jam** | Dengan testing di setiap fase |
