# AkiraOS vs Kairo — Perbandingan & Rekomendasi Adopsi

> Studi banding antara **AkiraOS** (`refs/AkiraOS`, v1.4.9 "Gl1tch") dan **Kairo**.
> Tujuannya: tahu kelebihan/kekurangan masing-masing, lalu pilih apa yang layak
> kita serap ke Kairo. Sumber: baca langsung source `refs/AkiraOS/` +
> <https://docs.akiraos.dev/>, dibandingkan dengan `firmware/` kita.
>
> **Kenapa relevan:** goals-nya mirip — bikin OS embedded yang **dinamis**. Tapi
> jalannya beda. AkiraOS sudah "lulus" membuktikan model OS-stabil + app-dinamis di
> hardware nyata (OSHWA certified), jadi ia jadi referensi bagus untuk arah Plan 37
> (dynamic app) kita.

---

## TL;DR

- **AkiraOS = Zephyr + WAMR.** Setiap app adalah modul `.wasm` ter-sandbox,
  di-deploy OTA tanpa reflash, dijaga **Capability Guard** (per-app permission
  enforced di setiap native call). Kuat di: keamanan, isolasi crash, OTA,
  portabilitas chip (ESP32/nRF/STM32/RISC-V). Lemah di: berat (Zephyr+WAMR+MCUboot),
  UI primitif (9 widget, gambar via framebuffer low-level), toolchain curam.
- **Kairo = C++17 runtime native + (Forge web ecosystem).** App **statis**
  (di-compile-in) atau JS embedded (QuickJS). Kuat di: UI modern (flex layout,
  resolution-independent, declarative ComponentApp), arsitektur bersih
  (core/platform/board/target, runtime capability check), simulator WASM di
  browser + remote KLP (BLE/USB/virtual-cable). Lemah di: app belum benar-benar
  dinamis, **capability hanya advisory (tidak di-enforce)**, tidak ada
  sandbox/isolasi crash app, tidak ada OTA/secure-boot, board masih sedikit.
- **Yang paling layak diserap:** (1) **manifest + capability enforcement per-app**,
  (2) **isolasi crash + auto-restart**, (3) **OTA install app ke device yang lagi
  jalan** (lewat KLP/HTTP), (4) **per-app memory quota & storage sandbox**,
  (5) **secure boot + rollback (MCUboot-style)**. Item 1–2 murah & berdampak besar
  begitu app jadi dinamis (Plan 37).

---

## 1. Positioning

| | **AkiraOS** | **Kairo** |
|---|---|---|
| Tagline | "WebAssembly Runtime for Microcontrollers" | "Hardware-agnostic embedded firmware runtime" |
| Basis | Zephyr RTOS 4.3 + WAMR 2.x | C++17 di atas ESP-IDF/Arduino (no RTOS framework wajib) |
| Unit "app" | `.wasm` module ter-sandbox, loadable saat runtime | C++ `ComponentApp` (compile-in) / JS string (QuickJS) |
| Cara app sampai ke device | **OTA**: `curl -F file=@app.wasm http://device/upload` | **Di-compile ke firmware** (atau embed sbg string) |
| Model keamanan | Zero-trust: WASM sandbox + Capability Guard + manifest | Capability **advisory** (`rt.capabilities().has(...)`) |
| Simulator | `native_sim` (host, SDL) | **WASM di browser** (firmware utuh) + remote KLP |
| Hardware | ESP32-S3/C3/C6/H2, nRF54L15, STM32, native | ESP32-S3 (skyrizz-e32, dev-board) + simulator |
| Pemakaian WASM | **App on-device** (WAMR di MCU) | **Simulator** (emscripten di browser) |
| Dynamism via | Modul `.wasm` + OTA | QuickJS JS app (embedded) + Plan 37 (.kapp, belum jadi) |

> Ironi menarik: **dua-duanya pakai WASM, tapi untuk hal berbeda.** AkiraOS
> menjalankan *app* sebagai WASM di atas MCU. Kairo menjalankan *seluruh firmware*
> sebagai WASM di browser (simulator). Jalur dinamis Kairo saat ini bukan WASM,
> tapi **QuickJS** (interpreter JS embedded).

---

## 2. Perbandingan Arsitektur (per subsistem)

| Subsistem | AkiraOS | Kairo | Catatan |
|---|---|---|---|
| **App model** | Dinamis: `.wasm` di-load dari `/lfs/apps/`, thread-per-app, max 8 installed / 2 running | **Statis**: app di-compile-in; JS app embedded string; `AppHostManager` single-slot | Gap terbesar. Kairo Plan 37 Fase 6 mau ke arah dinamis tapi belum ada. |
| **Isolasi & sandbox** | WASM linear-memory sandbox; fault app ditangkap di boundary; device tetap up | Native app jalan di Nema thread → fault native bisa nge-crash firmware. JS app lebih aman (QuickJS) | AkiraOS jauh lebih kuat untuk app tak-terpercaya. |
| **Capability** | Bitmask 26-bit per app, **di-enforce inline** tiap native call (~60ns), `-EACCES` kalau nolak, audit log | `CapabilityRegistry` (board → kapabilitas), tapi **advisory**: app cuma `has()`-cek, tak ada penolakan | Kairo perlu enforcement begitu app dinamis. |
| **Manifest** | JSON di custom section `.akira.manifest` (name, version, capabilities, memory_quota, restart policy) | `embedded_apps.h`: cuma `{id, name, js}` | Kairo butuh manifest yang lebih kaya. |
| **Memory** | Quota per app (atomic `memory_used` vs `memory_quota`), alloc PSRAM-first, chunked load | Tidak ada akuntansi memori per-app | — |
| **Storage** | `fs_manager`: SD→LittleFS→RAM fallback + **per-app storage isolation** (`/data/apps/<name>/`) | `IConfigStore` (NVS key-value) saja | Kairo belum punya FS abstraction / app-scoped storage. |
| **OTA** | App via HTTP `/upload`; firmware via MCUboot dual-slot + SHA-256 + signature + **rollback** | Forge `/flash` (esptool-js, full firmware) + registry `firmware.*`; **tak ada secure-boot/rollback**, tak ada "install app ke device hidup" | Kairo punya pipa transport bagus (KLP) tapi belum dipakai buat app-install. |
| **UI framework** | Custom retained-mode, **9 widget**, dirty-flag; app umumnya gambar via primitive framebuffer low-level (RGB565); LVGL opsional | **Flex layout + declarative `ComponentApp`/UiNode tree**, resolution-independent (Canvas logical scale), renderer + ViewDispatcher | **Kairo menang telak** di UI: lebih modern & portabel lintas resolusi. |
| **Input** | Tombol gaming (bitmask), diproses di shell/UI | **Action intent layer** (Prev/Next/Activate/Back), IKeyMap + gesture (short/long/double/chord), `validateFloor()` | Kairo lebih matang. |
| **HAL / board abstraction** | Compile-time `#if CONFIG_SOC_*` + Zephyr devicetree + driver registry | **Runtime capability check** ("never branch on board type"), layering core/platform/board/target | Pendekatan Kairo lebih bersih & dinamis; devicetree Zephyr lebih powerful tapi curam. |
| **Konektivitas** | HTTP server, BLE, USB, RF/LoRa/sub-GHz, "AkiraMesh", Matter/Thread (scaffolding) | KLP (BLE/USB-CDC/virtual-cable) + WiFi; RemoteService channel router | AkiraOS lebih luas (RF/mesh); Kairo lebih rapi (1 protokol KLP, banyak transport). |
| **Logging** | Zephyr log + shell | **Logger multi-sink** (Console+Memory) + Logs screen on-device + stream via KLP | Kairo lebih observable. |
| **Shell** | CLI lengkap (Zephyr shell, 2189 baris) — diagnostics, hw control | Tidak ada CLI shell (UI-driven) | AkiraOS punya, Kairo tidak (by design UI-first). |
| **Toolchain** | west + Zephyr SDK + WASI SDK (curam) | ESP-IDF/Arduino + CMake + bun (Forge) | Kairo lebih ringan untuk di-setup. |
| **Footprint** | Berat (Zephyr+WAMR+MCUboot) | Ringan (bisa bare ESP-IDF/Arduino) | — |

---

## 3. Kairo — Kelebihan & Kekurangan

### ✅ Kelebihan Kairo (yang JANGAN ditiru-mundur)
1. **UI jauh lebih modern.** Flex layout, declarative `ComponentApp`, `UiNode` tree,
   resolution-independent (`Canvas` logical scale). AkiraOS cuma 9 widget +
   framebuffer primitive. Ini aset besar — pertahankan.
2. **Arsitektur bersih & dinamis di level HAL.** core→platform→board→target +
   runtime capability check. Lebih elegan dari `#if CONFIG_SOC_*` ala Zephyr.
3. **Forge ecosystem unik.** Simulator WASM firmware-utuh di browser + remote KLP
   (BLE/USB/virtual-cable) dengan discovery list. AkiraOS tak punya padanannya
   (cuma native_sim SDL + HTTP upload).
4. **Input abstraction matang** (Action layer, gesture, keymap floor validation).
5. **Observability** (Logger multi-sink, Logs screen, stream KLP).
6. **Ringan & gampang di-setup** (ESP-IDF/Arduino + bun), tanpa Zephyr.
7. **Sudah punya jalur dinamis JS** (QuickJS `JsApp` + pipeline `embedded_apps.h`).

### ❌ Kekurangan Kairo (gap nyata vs AkiraOS)
1. **App masih statis.** Belum bisa install app ke device yang lagi jalan; semua
   di-compile-in / embed string. (Plan 37 Fase 6 = niat, belum kode.)
2. **Capability tidak di-enforce** — cuma advisory. Bahaya begitu ada app pihak ke-3.
3. **Tidak ada sandbox/isolasi crash untuk native app** — fault bisa bunuh firmware.
4. **Tidak ada per-app memory quota** maupun storage sandbox.
5. **Tidak ada OTA aman** (secure boot, signed image, A/B rollback). `/flash`
   sekarang tulis raw bin tanpa verifikasi tanda tangan.
6. **Board masih sedikit** (ESP32-S3 + sim) vs AkiraOS (5+ keluarga MCU).
7. **Tidak ada manifest app** (permissions/version/quota/restart-policy).

---

## 4. AkiraOS — Kelebihan & Kekurangan

### ✅ Kelebihan AkiraOS (kandidat adopsi)
1. **App benar-benar dinamis + OTA** tanpa reflash (`/upload` → `/lfs/apps`).
2. **Capability Guard** di-enforce di setiap native call (bitmask + audit log).
3. **Isolasi crash + auto-restart** (retry/backoff per manifest); device tetap hidup.
4. **Memory quota per app** + alloc PSRAM-first + chunked load (hemat SRAM peak).
5. **OTA aman**: MCUboot dual-slot, SHA-256, signature, rollback otomatis.
6. **Manifest** kaya (capabilities, quota, version, restart) embedded di binary.
7. **Portabilitas chip luas** (Xtensa/ARM/RISC-V) via WAMR target auto-select + AOT 10–50x.
8. **Storage berlapis** (SD→LittleFS→RAM) + per-app isolation.
9. **Settings terenkripsi** (AES-256-GCM) untuk rahasia (mis. wifi psk).

### ❌ Kekurangan AkiraOS (yang JANGAN ditiru)
1. **Berat**: Zephyr + WAMR + MCUboot = footprint flash/RAM besar.
2. **UI primitif**: 9 widget retained + framebuffer low-level; tak ada flex/responsive
   layout, tak resolution-independent. (Kairo unggul jauh.)
3. **HAL compile-time gating** (`#if SOC`) — kurang dinamis dibanding capability runtime.
4. **Rapuh di beberapa titik**: JSON parser hand-rolled, multipart HTTP manual.
5. **Toolchain curam** (west/Zephyr SDK/WASI SDK).
6. **Permukaan native API besar** (250+ fungsi) = attack surface besar.
7. **Tak ada browser simulator / protokol remote terpadu** seperti KLP.

---

## 5. Rekomendasi Adopsi (prioritas)

> Urut dari "murah & berdampak" → "besar & strategis". Tiap item dipetakan ke
> file/plan Kairo yang relevan.

### 🥇 P1 — App Manifest + Capability **Enforcement** (murah, wajib sebelum app dinamis)
- **Apa:** definisikan manifest per-app (`id, name, version, capabilities[],
  memoryQuotaKb, restart{enabled,maxRetries,delayMs}`). Saat app dilaunch,
  resolve `capabilities[]` jadi mask; **tolak** akses API yang tak diminta.
- **Kenapa:** begitu app bisa di-install dari luar (Plan 37), advisory capability
  tidak cukup — app jahat/buggy bisa pegang WiFi/storage tanpa izin. AkiraOS
  membuktikan enforcement inline murah (~60ns).
- **Di Kairo:** perluas `embedded_apps.h`/`.kapp` → tambah field manifest. Untuk
  `JsApp` (QuickJS), enforcement gampang: bridge JS→native (mis. `kairo.wifi.*`,
  `kairo.storage.*`) cek mask app sebelum eksekusi. Petakan ke
  `apps/js_app.h`, `system/capability_registry.h`, Plan 37.
- **Effort:** S–M. **Dampak:** tinggi (fondasi keamanan app dinamis).

### 🥈 P2 — Isolasi crash + auto-restart per app
- **Apa:** app yang fault tidak boleh menjatuhkan firmware; restart otomatis
  dengan retry/backoff + `crash_count`, state `ERROR→FAILED`.
- **Kenapa:** syarat OS "dinamis tapi stabil". QuickJS sudah meng-isolasi JS
  (exception ke-catch), jadi separuh jalan untuk jalur JS. Yang kurang: kebijakan
  restart + guard thread.
- **Di Kairo:** untuk `JsApp` bungkus eksekusi dengan try/catch + restart policy di
  `AppHostManager`. Untuk native app (Nema thread), tambah watchdog + supervisor.
  Petakan ke `app/app_host_manager.h`, `nema/task_runner.h`.
- **Effort:** M. **Dampak:** tinggi.

### 🥉 P3 — OTA "install app ke device yang lagi jalan" (lewat KLP / HTTP)
- **Apa:** push satu bundle app (.kapp/JS/wasm) ke device hidup, simpan ke storage,
  launch tanpa reflash — persis `/upload` AkiraOS.
- **Kenapa:** inti "dynamic OS". Kairo **sudah punya transport** (KLP +
  RemoteService EXT channel + Forge). Tinggal tambah channel/endpoint "app install".
- **Di Kairo:** tambah `ExtOp::AppInstall` di `services/remote_service.h` (terima
  bundle via KLP), simpan ke storage, daftarkan sbg `JsAppPlugin`. Atau endpoint
  HTTP di sisi device. Forge dapat halaman "Apps" untuk push. Petakan ke Plan 35/36
  + Plan 37 Fase 6.
- **Effort:** M–L. **Dampak:** tinggi (ini "the headline").
- **Prasyarat:** butuh storage (lihat P4) dan keamanan (P1).

### P4 — Storage abstraction + per-app sandbox + memory quota
- **Apa:** `IFileSystem` (mirror `fs_manager`: SD→internal→RAM fallback) + storage
  per-app (`/apps/<id>/...`) + akuntansi memori per app.
- **Kenapa:** app dinamis butuh tempat simpan bundle + data, dengan batas.
- **Di Kairo:** baru — `hal/filesystem.h` + impl LittleFS/SD (esp32) & memfs (host/wasm).
  Quota: hook alokasi di JsApp/native bridge. Petakan ke `hal/`, Plan 24 (Config).
- **Effort:** M–L. **Dampak:** sedang-tinggi (enabler P3).

### P5 — Secure boot + signed update + rollback (MCUboot-style)
- **Apa:** verifikasi tanda tangan firmware, dual-slot A/B, rollback kalau boot gagal.
- **Kenapa:** keandalan di lapangan (update tidak nge-brick). `/flash` sekarang
  tulis raw bin tanpa verifikasi.
- **Di Kairo:** ESP-IDF punya `app_update`/secure boot v2 + OTA partition. Tambah di
  `platforms/esp32` + manifest registry Forge tandatangani build. Petakan ke
  Plan 34/36.
- **Effort:** L. **Dampak:** sedang (penting kalau mau produksi).

### P6 — Settings terenkripsi untuk rahasia
- **Apa:** enkripsi value sensitif (wifi psk, token) — AES-GCM seperti AkiraOS.
- **Di Kairo:** layer enkripsi di `IConfigStore`/NVS (mbedtls ada di esp32).
- **Effort:** S–M. **Dampak:** sedang.

### P7 (opsional, strategis) — WAMR sebagai runtime app kedua
- Lihat §6. Hanya kalau butuh app multi-bahasa / perf native / sandbox memori sejati.

---

## 6. Keputusan Strategis: **QuickJS vs WAMR** untuk app dinamis

Ini fork-in-the-road terpenting. Dua-duanya valid; pilih sadar.

| Aspek | **QuickJS (jalur Kairo sekarang)** | **WAMR (jalur AkiraOS)** |
|---|---|---|
| Bahasa app | JavaScript saja | C/C++/Rust/AssemblyScript → `.wasm` (multi-bahasa) |
| Sandbox memori | Lewat interpreter (aman-ish) | **Linear memory sejati** (isolasi kuat) |
| Footprint | Kecil; sudah terintegrasi | Lebih besar (runtime + per-app heap) |
| Performa | Interpreter JS | Interpreter ~1x, **AOT 10–50x** |
| Capability di ABI | Bridge JS→native (kita yang buat) | NativeSymbol + cek mask di boundary |
| Ergonomi developer | Tinggi (JS, hot-reload via string) | Perlu WASI SDK + build step |
| Sudah ada di Kairo | ✅ `JsApp`, `embedded_apps.h` | ❌ (tapi kita sudah paham emscripten) |

**Rekomendasi:** untuk sekarang, **lanjutkan QuickJS** sebagai jalur app dinamis
utama (sudah ada, ringan, ergonomis), tapi **serap pola keamanan AkiraOS di
atasnya** (manifest + capability enforcement + isolasi crash + quota = P1/P2/P4).
Itu memberi 80% nilai "dynamic+secure OS" dengan ongkos jauh lebih murah daripada
mengadopsi Zephyr+WAMR.

Pertimbangkan **WAMR sebagai runtime kedua (P7)** nanti, hanya jika muncul kebutuhan
nyata: app berperforma tinggi, app non-JS, atau isolasi memori tingkat-produksi.
Arsitektur Kairo (`IApp`/`AppHost`) cukup fleksibel untuk menampung dua runtime app
berdampingan (JsApp + WasmApp) — keduanya hanya `IApp`.

> Catatan: **jangan adopsi Zephyr.** Itu akan membuang keunggulan Kairo (ringan, UI
> modern, layering bersih, Forge). Yang kita tiru dari AkiraOS adalah **pola**
> (manifest, capability enforcement, isolasi, OTA, secure boot) — bukan stack-nya.

---

## 7. Roadmap Usulan (mapping ke plan)

> Update 2026-06-08: setelah baca **Plan 37** (lagi dikerjakan AI builder, sudah
> Fase 5), ternyata sebagian besar rekomendasi **sudah tercakup** di sana. Plan baru
> difokuskan ke **gap yang belum / yang nge-block Plan 37**. Dua plan baru ditulis:

- **Plan 38 — Storage/Filesystem HAL + persistensi** (`38-storage-filesystem-appstore.md`)
  → rekomendasi **P4**. **Update 2026-06-09:** ternyata Plan 37 Fase 6 (OTA install)
  sudah **selesai *filesystem-free*** (`JsAppStore` daftarkan app live di RAM,
  volatile). Jadi Plan 38 **bukan blocker OTA** — perannya: **persistensi app tahan
  reboot** (flash-FS internal SPIFFS/LittleFS, **bukan** microSD), microSD untuk
  bulk libraries (Fase 7), storage file umum + sandbox/quota per-app, + dasar
  firmware-OTA staging (Plan 39).
- **Plan 39 — Firmware OTA: Secure Boot + Signed + Rollback** (`39-firmware-ota-secure-boot.md`)
  → rekomendasi **P5 + P6**. Dual-slot A/B, verifikasi tanda tangan, rollback otomatis,
  pull (WiFi) / push (KLP `Channel::Ota`), + settings terenkripsi.

### Sudah ada di Plan 37 (tak perlu plan baru, cukup pastikan tergarap)
- **P1 (manifest + capability):** Plan 37 sudah punya manifest (`kapp.json` `needs[]`)
  + system API **capability-gated**. ✅ Tinggal pastikan benar-benar **menolak**
  (enforce), bukan sekadar filter saat inject.
- **P2 (isolasi crash):** Plan 37 sudah punya **mem-limit + interrupt** (anti
  runaway) per context JS. Yang masih bisa ditambah: **restart policy**
  (retry/backoff) di `AppHostManager` — saran kecil untuk Plan 37/22, bukan plan baru.
- **P3 (install app OTA via KLP):** Plan 37 **Fase 6 ✅ SELESAI** (filesystem-free):
  `JsAppStore::installKapp` + `Channel::Ext` op `AppInstall (0x03)`; Forge push →
  app muncul live. Yang tersisa = **persistensi** (Plan 38) + UI upload Forge.

### Keputusan strategis (P7)
- **WAMR sebagai runtime kedua** = belum dibuat plan; tetap **opsional/nanti**.
  Lihat §6: lanjut QuickJS dulu, WAMR hanya kalau butuh app non-JS/perf tinggi.

**Yang TIDAK diubah:** UI (flex/ComponentApp), Forge simulator+remote KLP, layering
HAL, input Action layer, jalur QuickJS Plan 37 — semua sudah unggul / on-track.

### Urutan eksekusi yang disarankan
```
Plan 37 Fase 6 (app OTA install)  ✅ SELESAI (volatile, filesystem-free)
Plan 38 (filesystem)  ──persist──►  app OTA tahan reboot + microSD bulk (Plan 37 Fase 7)
                      └──prereq──►  Plan 39 Fase 0 (partisi storage vs A/B)
Plan 39 (firmware OTA) ── independen, paralel setelah layout partisi disepakati
```

---

### Lampiran — referensi file
- **AkiraOS:** `refs/AkiraOS/src/runtime/akira_runtime.c` (WAMR load/exec),
  `app_manager/app_manager.c` (lifecycle), `security.c` (capability),
  `manifest_parser.c`, `api/akira_export_api.c` (NativeSymbol),
  `connectivity/ota/web_server.c` (`/upload`), `ota/ota_manager.c` (MCUboot),
  `storage/fs_manager.c`, `ui/ui_framework.c`.
- **Kairo:** `firmware/core/include/kairo/app/{app,app_host,app_host_manager,component_app}.h`,
  `apps/js_app.h`, `apps/embedded_apps.h`, `system/capability_registry.h`,
  `services/remote_service.h`, `link/*`, `ui/{node,layout,renderer,canvas}.h`.
