# Report: Plans 47–59 Implementation

Semua yang dikerjakan dalam session ini untuk Plans 47–59.

---

## Status Akhir Per Plan

| Plan | Judul | Status |
|------|-------|--------|
| 47 | App Platform Architecture Overview | ✅ Done (doc only) |
| 48 | Nema System API IDL | ✅ Done (sebelum session) |
| 49 | SDK Binding Generation | ✅ Done (sebelum session) |
| 50 | UI SDK Model | ✅ Done (sebelum session) |
| 51 | Display Server Architecture | ✅ Done session ini |
| 52 | Aether UI SDK (SmartLabel/marquee) | ✅ Done session ini |
| 53 | Theming & Asset Packs (icon system) | ✅ Done session ini |
| 54 | Process Model (`run <app> \| B`) | ✅ Done (sudah ada sebelumnya) |
| 55 | Surface & Window Model | ✅ Done session ini |
| 56 | App Runtime Architecture (Native tier) | ✅ Done session ini |
| 57 | WASM Runtime | ⏸️ Deferred — wasm3 belum di-vendor |
| 58 | JS Runtime Process Model | ✅ Done session ini |
| 59 | App Manifest, Packaging & PAPP1 | ✅ Done session ini |

---

## Detail Per Plan

### Plan 51 — Display Server Architecture
**File yang diubah:**
- `firmware/core/include/nema/services/gui_service.h`
- `firmware/core/src/services/gui_service.cpp`

**Yang dikerjakan:**
- Tambah `registerServer(IDisplayServer* s)` ke `GuiService` — display server eksternal bisa didaftarkan secara dinamis (sebelumnya hanya hardcoded `aether_` dan `fbcon_`)
- Tambah `extraServers_` vector di `GuiService`
- `findServer()`, `requestServer()`, dan `serverNames()` sekarang juga cek `extraServers_`
- Refactor `requestServer()` untuk pakai `findServer()` secara internal (DRY)

---

### Plan 52 — Aether UI SDK (SmartLabel + marquee tick)
**File yang diubah/dibuat:**
- `firmware/core/include/nema/ui/node.h` — tambah `TextRole::Smart`
- `firmware/core/include/nema/ui/renderer.h` — deklarasi `setRenderTick()` / `renderTick()`
- `firmware/core/src/ui/renderer.cpp` — implementasi tick global + SmartLabel rendering
- `firmware/core/include/nema/ui/widgets.h` — deklarasi `SmartLabel()`
- `firmware/core/src/ui/widgets.cpp` — implementasi `SmartLabel()`
- `firmware/core/src/services/gui_service.cpp` — panggil `setRenderTick(now)` sebelum render tiap frame

**Yang dikerjakan:**
- `TextRole::Smart` ditambah ke enum — text node yang sadar ukuran
- `s_renderTick` static global di `renderer.cpp`, diset `GuiService` tiap frame lewat `setRenderTick(ms)`
- `paint()` di renderer sekarang punya parameter `inFocused` yang dipropagasi ke seluruh descendant dari focused node
- Smart text: `draw::ellipsis()` kalau tidak focused, `draw::marquee()` kalau focused (tick-driven)
- `SmartLabel(a, text)` builder — shortcut untuk `Text(a, text, TextRole::Smart)`

---

### Plan 53 — Theming & Asset Packs (Icon System)
**File yang dibuat:**
- `firmware/core/include/nema/ui/icon_pack.h`
- `firmware/core/src/ui/icon_pack.cpp`
- `firmware/core/include/nema/ui/node.h` — tambah `NodeType::Icon` + fields `iconBitmap/W/H`
- `firmware/core/include/nema/ui/widgets.h` — deklarasi `Icon()`
- `firmware/core/src/ui/widgets.cpp` — implementasi `Icon()`
- `firmware/core/src/ui/layout.cpp` — handle `NodeType::Icon` di `measure()`
- `firmware/core/src/ui/renderer.cpp` — handle `NodeType::Icon` di `paint()`

**Yang dikerjakan:**
- 16 built-in 8×8 XBM bitmaps (1-bit, row-major, MSB first):
  - `status.wifi`, `status.bt`, `status.battery`, `status.charging`
  - `feature.apps`, `feature.settings`, `feature.gpio`, `feature.subghz`, `feature.nfc`
  - `file.folder`, `file.file`, `file.generic`
  - `action.warning`, `action.info`, `action.ok`, `action.spinner`
- `findIcon(handle)` — lookup O(n) by string handle, returns `const IconDef*`
- `allIcons()` — null-terminated list untuk enumerasi
- `NodeType::Icon` — leaf node baru di tree; layout = fixed `iconW×iconH + padding`; renderer panggil `aether::ui::draw::icon()`
- `Icon(a, bitmap, w, h, padding)` builder
- `status_bar.cpp` diupdate — WiFi indicator sekarang pakai `status.wifi` XBM bitmap (bukan karakter "W")
- `app_list_screen.cpp` diupdate — tiap baris app sekarang tampilkan icon (`feature.apps` default, atau sesuai kategori) + SmartLabel untuk nama app

**Mapping kategori → icon handle:**
- `"SubGHz"` → `feature.subghz`
- `"NFC"` → `feature.nfc`
- `"GPIO"` → `feature.gpio`
- `"Settings"` → `feature.settings`
- default → `feature.apps`
- `iconPath` di manifest bisa langsung berisi handle (dicek duluan)

---

### Plan 55 — Surface & Window Model
**File yang dibuat:**
- `firmware/core/include/nema/ui/single_foreground_policy.h`
- `firmware/core/src/ui/single_foreground_policy.cpp`

**Yang dikerjakan:**
- `SingleForegroundPolicy` — concrete class implementasi `IWindowPolicy`
- Constructor menerima `AppHostManager&`
- `onSurfaceCreated(s)` → set `foreground_ = &s`
- `onSurfaceDestroyed(s)` → clear `foreground_` kalau match
- `visibleSurfaces(out)` → push `foreground_` kalau ada
- `focused()` → return `foreground_`
- Ini adalah v1 policy yang merepresentasikan perilaku single-foreground AppHostManager yang sudah ada

---

### Plan 56 — App Runtime Architecture
**File yang dibuat:**
- `firmware/core/include/nema/app/builtin_runtime.h` (di session sebelumnya)

**Yang dikerjakan session ini:**
- Update status doc — `IAppRuntime` + `JsRuntime` + `CBuiltinRuntime` complete
- `WasmRuntime` dinyatakan deferred ke Plan 57 (bukan gap, tapi keputusan eksplisit)
- `AppManifest` extended: `mode`/`category`/`iconPath`/`needs[]`/`apiVersion`

---

### Plan 58 — JS Runtime Process Model
**File yang diubah:**
- `firmware/core/src/js/js_api.cpp`
- `firmware/core/include/nema/js/js_engine.h`
- `firmware/core/src/apps/js_app.cpp`

**Yang dikerjakan:**
- `setProcessContext(ProcessContext* ctx)` di `JsEngine` — install global `process` ke QuickJS context
- `process.argv` → array dari `ctx->args()`
- `process.exit(n)` → panggil `ctx->requestExit(n)` + throw JS sentinel untuk unwind stack
- `process.stdout.write(s)` → `ctx->out().write()`
- `process.stdin.read()` → return `""` (stub)
- `js_app.cpp` panggil `eng_->setProcessContext(&ctx)` di `onStart()` sebelum `loadApp()`

**Fix ESP32 build:**
- `int code` → `int32_t code` di `process_exit()` (Xtensa: `int32_t = long`, bukan `int`)
- Format string `%d` → `%ld` dengan cast `(long)code` untuk portabilitas

---

### Plan 59 — App Manifest, Packaging & PAPP1 Installer
**File yang dibuat:**
- `firmware/core/include/nema/app/papp_installer.h`
- `firmware/core/src/app/papp_installer.cpp`

**File yang diubah:**
- `firmware/core/include/nema/runtime.h` — tambah `setFs(IFileSystem*)` / `fs()`
- `firmware/platforms/esp32/src/esp32_platform.cpp` — panggil `rt.setFs(&vfs_)`
- `firmware/platforms/wasm/src/wasm_platform.cpp` — panggil `rt.setFs(&vfs_)`
- `firmware/core/CMakeLists.txt` — tambah `papp_installer.cpp`, `icon_pack.cpp`, `single_foreground_policy.cpp`

**Yang dikerjakan:**
- `Runtime::setFs(IFileSystem*)` / `fs()` — accessor ke platform VFS; platform set di `registerDrivers()`
- `installPapp(Runtime& rt, const uint8_t* data, size_t len)`:
  - Parse PAPP1 via `parsePapp()`
  - Single-file → delegate ke `JsAppStore::installKapp()`
  - Bundle (TOC) → explode semua entries ke `/flash/apps/<id>/`, cari `.js` entry, panggil `JsAppStore::installApp()`
- `loadInstalledPapps(Runtime& rt)`:
  - Scan `/flash/apps/` di VFS
  - Tiap subdirektori: baca `manifest.json` + `app.js` (atau `main.js`)
  - Register ke `JsAppStore` sehingga app persisted survive reboot

---

## File Baru (Ringkasan)

| File | Plan |
|------|------|
| `include/nema/ui/icon_pack.h` | 53 |
| `src/ui/icon_pack.cpp` | 53 |
| `include/nema/ui/single_foreground_policy.h` | 55 |
| `src/ui/single_foreground_policy.cpp` | 55 |
| `include/nema/app/papp_installer.h` | 59 |
| `src/app/papp_installer.cpp` | 59 |

## File yang Diubah (Ringkasan)

| File | Plan | Perubahan |
|------|------|-----------|
| `include/nema/ui/node.h` | 52, 53 | `TextRole::Smart`, `NodeType::Icon`, icon fields |
| `include/nema/ui/renderer.h` | 52 | `setRenderTick()`, `renderTick()` |
| `src/ui/renderer.cpp` | 52, 53 | tick global, `inFocused` propagation, Smart text, Icon paint |
| `include/nema/ui/widgets.h` | 52, 53 | `SmartLabel()`, `Icon()` |
| `src/ui/widgets.cpp` | 52, 53 | impl `SmartLabel()`, `Icon()` |
| `src/ui/layout.cpp` | 53 | `NodeType::Icon` measure |
| `src/ui/status_bar.cpp` | 53 | WiFi → XBM icon |
| `include/nema/services/gui_service.h` | 51 | `registerServer()`, `extraServers_` |
| `src/services/gui_service.cpp` | 51, 52 | `registerServer()`, `setRenderTick()`, lookup refactor |
| `include/nema/runtime.h` | 59 | `setFs()`, `fs()`, forward decl `IFileSystem` |
| `src/js/js_api.cpp` | 58 | `int32_t` fix, `%ld` fix |
| `include/nema/screens/app_list_screen.h` | 52, 53 | `icons_` vector |
| `src/screens/app_list_screen.cpp` | 52, 53 | SmartLabel + Icon per baris |
| `platforms/esp32/src/esp32_platform.cpp` | 59 | `rt.setFs(&vfs_)` |
| `platforms/wasm/src/wasm_platform.cpp` | 59 | `rt.setFs(&vfs_)` |
| `core/CMakeLists.txt` | — | tambah 3 file baru ke `NEMA_CORE_SRCS` |

---

## Build & Test

- Host build (Unix Makefiles): ✅ clean, 0 errors
- Test suite: ✅ 10/10 passed
- ESP32 build: ✅ (setelah fix `int32_t` / `%ld` di `js_api.cpp`)
- WASM build: tidak diverifikasi ulang session ini

---

## Yang BELUM Dikerjakan (Noted)

- **Plan 57** — WasmRuntime: deferred, wasm3 belum di-vendor
- **Plan 60 Fase 5D** — Built-in apps (counter, clock, stopwatch, wifi, bluetooth, ticker, task_demo, touch_test, camera, ui_showcase) belum ditulis ulang ke Plan 60 style — masih pakai widget lama
- **Plan 60 Fase 0** — `.bak/aether-v1/` sudah ada, tapi apps lama belum dihapus dari source tree (apps masih compile dan muncul di launcher dengan UI lama)
