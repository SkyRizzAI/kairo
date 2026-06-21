# Plan 84 — App Runtime Parity: C++, JS, WASM

## Goals

Membuat ketiga jenis app (C++ built-in, JS external, WASM external) berperilaku sama
dari sudut pandang developer dan user:

1. **Parity API** — ketiga tipe akses storage, log, device info lewat interface yang sama
2. **CLI mode** — app bisa jalan headless (args in → stdout out), tidak wajib punya UI
3. **UI mode** — app bisa buka aether canvas, pakai ScrollView/ListView/dll
4. **Hybrid** — satu app bisa jalan di kedua mode tergantung dari mana diluncur
5. **Launcher parity** — ketiga tipe muncul di aether app list dengan icon, title, id dari manifest
6. **SDK parity** — `packages/app-sdk` bisa build + bundle JS dan WASM jadi `.papp`
7. **WASM guardrail** — sampai UI selesai, WASM `.papp` kasih error jelas bukan crash diam-diam

---

## Background

### Model yang dimaksud: Unix process model

```
App = proses
  ├─ CLI mode:    runProcess(ProcessContext) → args in, stdout out, exit code
  ├─ UI mode:     run(AppContext)            → aether canvas, input mailbox
  └─ Hybrid:      keduanya — launcher buka UI, CLI launch buka headless
```

Ini sama persis seperti Linux: `grep` jalan di terminal, `gedit` buka window,
`firefox --headless` bisa keduanya. Di Palanu: launcher = display manager,
CLI shell = terminal.

### State sebelum plan ini

Dari audit plan 84 session awal:

| Dimensi | C++ | JS | WASM |
|---------|-----|----|------|
| Build + bundle | ✅ | ✅ | ❌ |
| Muncul di launcher | ✅ | ✅ | ❌ crash |
| UI mode | ✅ | ✅ | ❌ no-op |
| CLI mode | ⚠️ runProcess ada, tidak pernah dispatch | ⚠️ process.* ada, tidak ada launch path | ✅ WASI headless |
| Storage API | ✅ AppStorage | ✅ nema.storage.* | ❌ |
| nema.* API | ✅ | ✅ | ❌ |
| Icon di launcher | ❌ | ❌ | ❌ |

Yang sudah ada tapi belum diwire:
- `AppMode { Cli, Ui, Hybrid }` — enum di `app_manifest.h`, tidak pernah dibaca dari manifest JSON, tidak pernah dicek saat launch
- `IApp::runProcess(ProcessContext&)` — ada di `app.h` (Plan 54), default no-op, tidak pernah dispatch dari launcher
- `process.*` di JS API — `argv`, `exit`, `stdout.write` ada di `nema_host_impl.cpp` tapi tidak ada CLI launch path
- `IAppRuntime` adapters (`JsRuntime`, `CBuiltinRuntime`) — dead code, Plan 56 setengah jalan
- `AppMode` field di `AppManifest` — tidak dibaca dari `manifest.json` di `installFromDir()`

---

## Fase

### Fase 1 — CLI + UI dispatch (C++ dan JS) ✅ setelah ini

Mengaktifkan infrastruktur yang sudah ada tapi belum diwire.

**1a. Baca `mode` dari `manifest.json`**

File: `firmware/core/src/app/papp_installer.cpp`, fungsi `installFromDir()`

```cpp
// sekarang: mode tidak dibaca
// target:
std::string modeStr = mj.value("mode", "ui");
if      (modeStr == "cli")    m.mode = AppMode::Cli;
else if (modeStr == "hybrid") m.mode = AppMode::Hybrid;
else                          m.mode = AppMode::Ui;
```

**1b. CLI dispatch di `AppRegistry::launch()`**

File: `firmware/core/src/app/app_registry.cpp`

Sekarang `launch()` selalu routing ke `AppHostManager` (UI path). Perlu tambah:

```cpp
// Jika mode == Cli → spawn thread dengan runProcess(ProcessContext)
// Jika mode == Hybrid → launch dari launcher = UI; bisa juga dipanggil via CLI
// Jika mode == Ui (default) → behaviour existing, tidak berubah
```

`ProcessContext` sudah ada (Plan 54). Yang perlu dibuat: path `AppRegistry::launchProcess()`
yang spawn thread baru, beri `ProcessContext` berisi `argv`, `stdout` sink, `exit` handler,
tanpa canvas, tanpa input mailbox.

**1c. CLI output routing**

Saat app berjalan di CLI mode dan diluncur dari aether (bukan dari shell), stdout perlu
dirouting ke suatu tempat. Opsi:
- **Session log** — output masuk ke `rt.log()` dengan level `info`, tag = bundle id
- **In-app terminal buffer** — launcher buka panel kecil yang show stdout (seperti terminal embed)

Decision: **session log dulu** (lebih simpel, tidak butuh UI baru). Terminal buffer bisa
jadi Fase 4.

**1d. JS process args**

`process.argv` di `nema_host_impl.cpp` sekarang return hardcoded empty. Wire ke
`ProcessContext::argv` yang diset saat `launchProcess()`.

**Checklist Fase 1:**
- [ ] `installFromDir()` baca field `mode` dari manifest JSON
- [ ] `AppManifest` field `mode` diset saat install dari dir dan dari embedded
- [ ] `AppRegistry::launchProcess(id, argv[])` — fungsi baru untuk CLI launch
- [ ] Thread spawn di `launchProcess` pakai `runProcess(ProcessContext)`, bukan `run(AppContext)`
- [ ] `AppMode::Cli` app → `launchProcess` dari launcher (bukan `AppHostManager`)
- [ ] `AppMode::Ui` app → existing path, tidak berubah
- [ ] `AppMode::Hybrid` app → launcher pakai UI path; CLI launch pakai process path
- [ ] JS `process.argv` wire ke `ProcessContext::argv`
- [ ] JS `process.stdout.write` routing ke `rt.log()` saat di CLI mode
- [ ] Test: C++ app dengan `runProcess()` bisa jalan dari launcher dan output ke log
- [ ] Test: JS app dengan `mode: "cli"` bisa jalan dari launcher, output ke log

---

### Fase 2 — Launcher parity: manifest icon, title, id

Saat ini launcher hanya punya title dan id. Icon tidak ada di pipeline sama sekali.

**2a. Field `icon` di `AppManifest`**

```cpp
struct AppManifest {
    // ... existing fields
    std::string iconPath;  // relative ke .papp dir, e.g. "icon.png" atau "icon.raw"
};
```

**2b. Baca `icon` dari `manifest.json`**

```json
{
  "id": "com.example.myapp",
  "name": "My App",
  "icon": "icon.png",
  "mode": "ui",
  "runtime": "js"
}
```

**2c. Icon format**

Pilihan format yang realistis untuk embedded:
- **Raw RGB565** — langsung blit ke canvas, tidak perlu decoder. `icon.raw` = 32×32 × 2 bytes = 2 KB.
- **PNG** — butuh decoder (lodepng/miniz), lebih fleksibel tapi +8–15 KB binary size.

Decision: **raw RGB565 dulu** (sudah ada `canvas.drawBitmap()`), PNG bisa Fase 4.

**2d. `AppListScreen` tampilkan icon**

Icon di-load saat `installFromDir()` dan disimpan sebagai `std::vector<uint8_t>` di
`AppManifest` (atau di-load lazy dari path). `AppListScreen` blit icon sebelum judul app.

**2e. `packages/app-sdk` — icon pipeline**

Build tool sudah ada. Tambah: jika `icon.png` ada di source dir, convert ke RGB565 raw
saat build. Bisa pakai `sharp` (sudah likely ada di bun ecosystem) atau `ffmpeg`.

**Checklist Fase 2:**
- [ ] `AppManifest` tambah field `iconPath` dan `iconData` (raw bytes)
- [ ] `installFromDir()` baca `icon` dari manifest, load raw bytes dari file
- [ ] `AppListScreen` tampilkan icon (32×32 fallback ke placeholder jika tidak ada)
- [ ] `embedded_apps.h` format tidak berubah (embedded apps tidak punya icon untuk sekarang)
- [ ] `packages/app-sdk/bin/build.ts` convert icon PNG → raw RGB565 jika ada
- [ ] Test: app dengan icon muncul dengan icon di launcher

---

### Fase 3 — Fix bugs aktif (C++ dan JS)

Bug yang ada sekarang, blocking parity.

**3a. `JsAppStore::apps_` vector reallocation**

File: `firmware/core/include/nema/apps/js_app_store.h`

```cpp
// sekarang — BERBAHAYA: realloc invalidates pointers di AppRegistry
std::vector<std::unique_ptr<JsApp>> apps_;

// fix: stable container, pointer tidak pernah invalidate
std::list<std::unique_ptr<JsApp>> apps_;
// atau: pre-reserve atau pakai factory dengan stable heap alloc
```

Saat ini hanya 2 embedded apps jadi belum crash, tapi install app ke-9 akan invalidate
pointer ke app 1–8 yang sudah di-register ke `AppRegistry`.

**3b. `BadUsbApp` ke class yang benar**

File: `firmware/core/include/nema/apps/bad_usb_app.h`

`BadUsbApp` inherit dari `ComponentScreen` (system UI, GUI thread) bukan `ComponentApp`
(app, thread sendiri). Efek:
- Tidak terdaftar di `AppRegistry`, tidak muncul di launcher
- Jalan di GUI thread → HID I/O bisa freeze UI

Fix: migrate ke `ComponentApp`. Ini perubahan besar karena architecture HID berbeda
antara ComponentScreen dan ComponentApp.

**3c. `manifest.json` field `runtime` dispatch ke WASM**

File: `firmware/core/src/app/papp_installer.cpp`

Saat ini `installFromDir()` selalu call `JsAppStore::installApp()` tanpa cek `runtime`.
WASM `.papp` akan di-launch sebagai JS → crash.

Fix sementara (Fase 3): detect `runtime: "wasm"`, log error jelas, return false.
Fix final (Fase 5): route ke WASM runtime.

**Checklist Fase 3:**
- [ ] `JsAppStore::apps_` ganti ke `std::list<unique_ptr<JsApp>>`
- [ ] `BadUsbApp` migrate dari `ComponentScreen` ke `ComponentApp`
- [ ] `installFromDir()` cek `runtime` field, tolak WASM dengan error jelas (bukan crash)
- [ ] Test: install 10+ JS apps tidak crash
- [ ] Test: BadUSB muncul di launcher, jalan di thread sendiri

---

### Fase 4 — WASM parity (effort besar, plan terpisah)

WASM sudah headless via WASI. Yang kurang: UI + nema.* API.

**4a. WASM nema.* API bridge**

Ekspos storage, log, device info sebagai WASM imported functions. Konsepnya sama
dengan `nema_api_quickjs.gen.cpp` tapi untuk WASM imports — WASM module declare
`(import "nema" "storage_fs_write_file" (func ...))`, host fulfill.

Ini butuh extend IDL generator untuk emit WASM import table, bukan hanya QuickJS.

**4b. `WasmRuntime::runUi()`**

Saat ini no-op. WASM module perlu bisa call aether primitives via host imports:
`aether_view_begin`, `aether_text_label`, dll — sama persis dengan `aether_abi.cpp`
tapi via WASM import table.

**4c. Fix bundle size hardcode**

File: `firmware/core/src/wasm/wasm_runtime.cpp`

```cpp
// sekarang — SALAH: potong bundle di 1024 bytes
engine.load(reinterpret_cast<const uint8_t*>(bundle), 1024);

// fix: gunakan entry size dari PappEntry
engine.load(reinterpret_cast<const uint8_t*>(bundle), bundleSize);
```

**4d. SDK build toolchain untuk WASM**

`packages/app-sdk` tambah target `wasm` — pakai wasi-sdk atau Emscripten.
Template minimal: C app dengan `main(int argc, char* argv[])` yang bisa
call `nema_log()`, `nema_storage_write()`, dll via imported functions.

**Checklist Fase 4:**
- [ ] IDL generator emit WASM import table (baru, extend `packages/idl/src/emit/`)
- [ ] `WasmEngine` expose nema.* imports saat load
- [ ] `WasmRuntime::runUi()` implementasi: feed frame dari WASM ke AppHost
- [ ] Fix bundle size hardcode (trivial, fix dulu)
- [ ] `installFromDir()` route `runtime: "wasm"` ke `WasmRuntime` bukan `JsAppStore`
- [ ] `packages/app-sdk` tambah WASM build template
- [ ] Test: WASM app headless jalan dengan storage API
- [ ] Test: WASM app UI muncul di launcher

---

## File yang terdampak

| File | Fase | Perubahan |
|------|------|-----------|
| `firmware/core/src/app/app_registry.cpp` | 1 | Tambah `launchProcess()`, dispatch berdasar `AppMode` |
| `firmware/core/src/app/papp_installer.cpp` | 1, 3 | Baca `mode` dan `runtime` dari manifest; tolak WASM |
| `firmware/core/include/nema/app/app_manifest.h` | 1, 2 | Tambah `iconPath`, `iconData`; `mode` sudah ada |
| `firmware/core/src/js/nema_host_impl.cpp` | 1 | Wire `process.argv` dan `process.stdout.write` ke ProcessContext |
| `firmware/core/include/nema/apps/js_app_store.h` | 3 | `apps_` → `std::list` |
| `firmware/core/include/nema/apps/bad_usb_app.h` | 3 | Migrate ke `ComponentApp` |
| `firmware/core/src/wasm/wasm_runtime.cpp` | 4 | Fix bundle size; implement `runUi()` |
| `firmware/core/src/app/app_list_screen.cpp` | 2 | Tampilkan icon |
| `packages/idl/src/emit/` | 4 | Tambah WASM import emitter |
| `packages/app-sdk/bin/build.ts` | 2 | Icon PNG → raw RGB565 |

---

## Status

| Fase | Status |
|------|--------|
| Fase 1 — CLI + UI dispatch | `[ ]` not started |
| Fase 2 — Launcher icon | `[ ]` not started |
| Fase 3 — Bug fixes aktif | `[ ]` not started |
| Fase 4 — WASM parity | `[ ]` not started |
