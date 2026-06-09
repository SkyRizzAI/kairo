# 00 — MVP Plan Overview

> Index & ground rules untuk seri perencanaan **Kairo MVP**.
> Master plan lengkap ada di [`../concept_plan.md`](../concept_plan.md). Dokumen di folder ini memecah master plan menjadi stage-stage kecil yang bisa dikerjakan satu per satu.

---

## 0. Tier Hardware (penamaan resmi — biar tidak bingung)

Kairo punya **3 tier** hardware. Penamaan ini dipakai konsisten di seluruh docs & kode:

| Tier | Nama | Apa itu | Board id (`IBoard::name()`) | Status |
|---|---|---|---|---|
| 1 | **Simulator** | Virtual — jalan di host/web, dummy driver. Tidak ada hardware. | `simulator` | ✅ jalan |
| 2 | **Kairo Dev Board** | Hardware testing **sementara**: ESP32-S3-WROOM-1 + e-ink 2.7" 264×176 + 6 tombol TCA9534. Ini **device bekas projek sebelumnya** yang kebetulan dimiliki developer — dipakai untuk uji firmware di hardware nyata sampai V1 ada. Pinout/tombol = ref [`kairo-test-concept-esp32-s3-wroom-1-eink`](../../refs/kairo-test-concept-esp32-s3-wroom-1-eink). | `dev-board` | 🔜 plan 16–18 |
| 3 | **Kairo Board V1** | **PCB custom buatan sendiri** — target produk akhir Kairo. Belum didesain. Akan reuse platform `esp32` + Core yang sama; cuma beda board layer (pin/komponen). | `kairo-board-v1` | ⏳ future (post-M6) |

**Prinsip penting:** Core & platform `esp32` **tidak peduli** board mana. Yang beda antar Dev Board ↔ V1 hanya **board layer** (pin map, komponen terpasang). Jadi semua kerja di Dev Board sekarang langsung kepakai di V1 nanti — tinggal ganti board.

> Catatan: master plan (`concept_plan.md`) menyebut "DevKit S3 Board". Itu **disederhanakan** jadi **Kairo Dev Board** di sini, karena developer pakai device ESP32-S3-WROOM-1 + e-ink yang sudah ada (bukan devkit terpisah).

---

## 1. Tujuan MVP

> **Core Runtime (C++) yang bisa boot & berjalan di dalam Kairo Simulator, lengkap dengan dummy driver — tanpa hardware sama sekali.**

Yang **masuk** MVP:

- Core Runtime C++ (hardware-agnostic): boot flow, Logger, Event Bus, Service Container, Service Manager, System Introspection.
- **Simulator Platform** + **Simulator Board** + **Simulator Target** dengan **dummy/mock driver**.
- Bridge stdio↔WebSocket + Simulator Web UI dengan 4 panel: **Logs, Events, Services, Controls**.

Yang **DITUNDA** (bukan MVP, jangan dikerjakan dulu):

- Kairo Dev Board & Kairo Board V1, platform ESP32, driver hardware asli (lihat §0 Tier Hardware).
- UI Runtime, Screen System, Status Bar, Home Screen (display rendering).
- Plugin Runtime, Notification Manager, OTA.
- Radio/NFC/RFID/IR/Audio nyata. Panel Display / Hardware Registry / Capability Registry di web (data model boleh disiapkan, panel-nya nanti).

Pemetaan ke milestone master plan (§32): MVP ≈ **Milestone 1 (Core Foundation) + 2 (Observability) + 3 (Simulator)**. Milestone 4–7 di luar scope.

---

## 2. Keputusan arsitektur (dikunci)

| Topik | Keputusan |
|---|---|
| Bahasa Core | **C++17**, hardware-agnostic, dipakai ulang persis di firmware ESP32 nanti. |
| Build | **CMake**, build native host (clang di macOS) → executable `kairo-sim`. |
| Cara core "jalan di simulator" | Target `simulator` di-build sebagai **binary native host** + dummy driver. |
| Bridge core ↔ web | **Tidak pakai WebSocket di C++.** Binary baca command dari **stdin** & tulis telemetry ke **stdout** sebagai **JSON-lines**. Proses Bun yang **spawn** binary itu lalu relay stdio ↔ WebSocket ke browser. |
| Web UI | `packages/simulator` (dipertahankan, **tidak** di-rename ke `simulator-web`). Bun + `Bun.serve` (WebSocket) + React, tanpa Vite/Express/ws. |
| JSON di C++ | Vendor single-header `nlohmann/json` di `firmware/vendor/nlohmann/json.hpp`. |

### Diagram bridge

```text
┌────────────────── packages/simulator (Bun) ──────────────────┐
│  Browser (React)  ──WebSocket──►  Bun.serve relay            │
│        ▲                                │                     │
│        └──────────────WebSocket─────────┘                     │
│                          │ stdin (commands)  ▲ stdout (telemetry)
└──────────────────────────┼───────────────────┼───────────────┘
                           ▼                   │  JSON-lines
                  ┌──────────────── kairo-sim (C++ native) ─────┐
                  │  Target simulator → Board simulator →        │
                  │  Platform simulator (dummy drivers) → Core   │
                  └──────────────────────────────────────────────┘
```

---

## 3. Struktur repo (target akhir MVP)

```text
kairo/
├─ firmware/
│  ├─ core/                     # C++ runtime, TIDAK kenal platform/hardware apapun
│  │  ├─ include/kairo/         # public headers (interface)
│  │  ├─ src/
│  │  └─ CMakeLists.txt
│  ├─ platforms/
│  │  └─ simulator/             # host platform: dummy driver + stdio I/O
│  ├─ boards/
│  │  └─ simulator/             # deklarasi hardware virtual
│  ├─ targets/
│  │  └─ simulator/             # main(): rakit Runtime, baca stdin/tulis stdout
│  ├─ vendor/nlohmann/json.hpp  # single-header JSON
│  ├─ tools/                    # script bantu (build/run)
│  └─ CMakeLists.txt            # top-level host build
├─ packages/
│  └─ simulator/                # React web UI + Bun relay (sudah ada)
├─ docs/
│  ├─ concept_plan.md
│  └─ plans/                    # ← folder ini
└─ package.json                 # bun workspaces (sudah ada)
```

---

## 4. Daftar stage & urutan

Kerjakan **berurutan**; tiap dok punya `Depends on`. Centang status di sini saat selesai.

### Phase 1 — MVP: Core + Simulator (M1+M2+M3)

| # | Dokumen | Fitur / Stage | Depends on | Status |
|---|---|---|---|---|
| 01 | [`01-repo-build-foundation.md`](01-repo-build-foundation.md) | Struktur repo + CMake host build + wiring bun workspace | — | ✅ |
| 02 | [`02-core-runtime-boot-flow.md`](02-core-runtime-boot-flow.md) | Core skeleton, interface dasar, Runtime + Boot Flow | 01 | ✅ |
| 03 | [`03-logger-service.md`](03-logger-service.md) | Logger Service (levels, entry, sinks) | 02 | ✅ |
| 04 | [`04-event-bus.md`](04-event-bus.md) | Event Bus | 02 | ✅ |
| 05 | [`05-service-container-manager.md`](05-service-container-manager.md) | Service Container (DI) + Service Manager (lifecycle) | 02,03,04 | ✅ |
| 06 | [`06-hal-simulator-platform.md`](06-hal-simulator-platform.md) | HAL interface + Platform layer + Simulator Platform (dummy driver) | 03,04,05 | ✅ |
| 07 | [`07-board-target-simulator.md`](07-board-target-simulator.md) | Board + Target simulator + sample background services | 05,06 | ✅ |
| 08 | [`08-system-introspection.md`](08-system-introspection.md) | SystemInfo + Hardware Registry + Capability Registry | 06,07 | ✅ |
| 09 | [`09-stdio-bridge-protocol.md`](09-stdio-bridge-protocol.md) | Protokol JSON-lines (stdin/stdout) + sink/command di sisi C++ | 03,04,05,07 | ✅ |
| 10 | [`10-simulator-web-ui.md`](10-simulator-web-ui.md) | Bun relay + React shell + panel Logs/Events/Services/Controls | 09 | ✅ |
| 11 | [`11-mvp-integration-run.md`](11-mvp-integration-run.md) | Integrasi end-to-end + script `bun run sim` + run guide | semua | ✅ |

### Phase 2 — Plugin Runtime (M4)

| # | Dokumen | Fitur / Stage | Depends on | Status |
|---|---|---|---|---|
| 12 | [`12-plugin-runtime.md`](12-plugin-runtime.md) | IPlugin, PluginManager, PluginContext, HelloPlugin sample | 01–11 | ✅ |

### Phase 3 — UI Runtime (M5)

| # | Dokumen | Fitur / Stage | Depends on | Status |
|---|---|---|---|---|
| 13 | [`13-display-hal-sim.md`](13-display-hal-sim.md) | IDisplayDriver 1-bit + Canvas + BitmapFont 5×8 + SimDisplay 264×176 + web panel | 06,09,10 | ✅ |
| 14 | [`14-ui-runtime.md`](14-ui-runtime.md) | Retro UI: Key enum, IScreen, ViewDispatcher (stack + event-driven render) | 12,13 | ✅ |
| 14b | — | Built-in components: Label/Button/Row/Col/MenuItem/HRule + size (xs..2xl) + React mirror | 13,14 | ✅ |
| 15 | [`15-status-bar-home-screen.md`](15-status-bar-home-screen.md) | StatusBar 1 baris + HomeScreen ASCII art + app list (Flipper Zero style) | 12,13,14 | ✅ |

### Phase 4 — Kairo Dev Board (ESP32-S3 + e-ink, M6)

> Tier 2 hardware (§0). Testing real sementara di ESP32-S3-WROOM-1 + e-ink. Semua ini reuse di V1 nanti — beda cuma board layer.

| # | Dokumen | Fitur / Stage | Depends on | Status |
|---|---|---|---|---|
| 16 | [`16-esp32-platform.md`](16-esp32-platform.md) | Esp32Platform, Esp32Clock, Esp32WifiDriver, ESP-IDF dual-build (battery di-skip, tak ada HW) | 01–11 | ✅ |
| 17 | [`17-dev-board.md`](17-dev-board.md) | Kairo Dev Board (`dev-board`): board_config.h pinout, TCA9534 6-button → Key | 16 | ✅ |
| 18 | [`18-esp32-eink-display.md`](18-esp32-eink-display.md) | EinkDisplay (GxEPD2 GDEY027T91 264×176 1-bit), full refresh | 13,16,17 | ✅ |

> Pinout & button map dari ref `kairo-test-concept-esp32-s3-wroom-1-eink/firmware/main/badge_pins.h`. Tombol pakai `Key` enum yang **sama** dengan simulator. Arduino libs (GxEPD2/Adafruit) di-vendor dari `refs/oniondao-badge/software/components` ke `firmware/vendor/arduino-libs/`.
> Status ✅ = `idf.py build` sukses (binary 1.05 MB) **dan firmware sudah di-flash + jalan di perangkat fisik** (dikonfirmasi 2026-05-31). Flash: `bun run flash:esp32`.

### Phase 5 — Nema Kernel, App Model & Connectivity (M6.5–M7)

> Fondasi multi-thread (Nema kernel) + model app per-thread + WiFi/HTTP/keyboard/networked apps.
> Inti: **tidak ada operasi yang membekukan UI** — display, input, dan kerja berat (scan, download)
> jalan di thread terpisah.

| # | Dokumen | Fitur / Stage | Depends on | Status |
|---|---|---|---|---|
| 19 | [`19-freertos-foundation.md`](19-freertos-foundation.md) | Async display task + AsyncEventPoster (offload flush e-ink, race WiFi aman) | 16,17 | ✅ HW |
| 19.5 | [`19.5-nema-kernel.md`](19.5-nema-kernel.md) | Nema kernel: `nema::Thread` + `MessageQueue` + `TaskRunner` + Input-thread | 19 | ✅ HW (F0–1+TaskRunner) |
| 19.6 | [`19.6-nema-app-model.md`](19.6-nema-app-model.md) | GuiService thread + `IApp`/`AppHost`/`AppContext` + migrasi semua app | 19.5 | ✅ build (HW: A,B) |
| 20 | [`20-wifi-networking.md`](20-wifi-networking.md) | WifiApp (scan/connect non-blocking) + HAL extend + sim "router" + esp32 driver+NVS | 19.5,19.6,16,17 | ✅ build, sim ✓ |
| 23 | [`23-ticker-keyboard-http.md`](23-ticker-keyboard-http.md) | IHttpClient HAL + VirtualKeyboard (QWERTY) + TickerApp (BTC/USD non-blocking) | 19.5,19.6,20 | ✅ build, sim ✓ |
| 34 | [`34-bluetooth-core-ble.md`](34-bluetooth-core-ble.md) | **Connectivity Foundation (Layer 1)**: BLE (controller+adapter, pairing LE Secure+numeric, bond, BluetoothApp) + USB composite (CDC; MSC/HID future). Fondasi multi-fungsi: remote, audio, storage, custom-app data | 19.5,19.6,16,24,30 | ☐ |

### Phase 6 — UX Polish (M7, belum dikerjakan)

| # | Dokumen | Fitur / Stage | Depends on | Status |
|---|---|---|---|---|
| 21 | [`21-screen-sleep-lock.md`](21-screen-sleep-lock.md) | Screen sleep otomatis + lock screen | 14,15,18 | ☐ |
| 22 | [`22-app-pause-resume.md`](22-app-pause-resume.md) | Pause/resume app (hold Cancel), "Continue: <app>" di launcher | 19.6,15 | ☐ |

### Phase 7 — Kairo Board V1 (PCB custom, future)

> Tier 3 hardware (§0). PCB buatan sendiri — belum didesain. Reuse platform `esp32` + Core; hanya board layer baru (`kairo-board-v1`).

| # | Dokumen | Fitur / Stage | Depends on | Status |
|---|---|---|---|---|
| — | _(belum ditulis)_ | Skematik PCB, board_config V1, bring-up, validasi | 16–18 | ⏳ future |

### Phase 8 — Board Profile & Ecosystem Foundation (M9)

> Fondasi data model untuk ekosistem tooling (Kairo Forge / Studio). Board mendeskripsikan layout fisik komponen, device render ASCII visualization, JSON export untuk web tooling.

| # | Dokumen | Fitur / Stage | Depends on | Status |
|---|---|---|---|---|
| 33 | [`33-board-profile.md`](33-board-profile.md) | BoardProfile data model + AsciiRenderer + About visualization + JSON export | 08,14,27 | ☐ |
| 35 | [`35-kairo-link-forge-remote.md`](35-kairo-link-forge-remote.md) | **Remote Layer (Layer 2, device-side)**: KLP codec + ILinkTransport (BLE/USB/virtual-cable) + RemoteService (screen-tap/input/log/system) + WASM firmware target | 34,13,19.5 | ☐ (codec TS ✓) |
| 36 | [`36-forge-foundation.md`](36-forge-foundation.md) | **Kairo Forge (Layer 3, web client)**: SvelteKit+Tailwind+shadcn+tRPC, simulator (→WASM), RemoteSession (Web Bluetooth/Serial/virtual-cable), /remote (remote device ATAU sim), /flash, OTA | 35,34,09,10,26,33 | 🚧 Fase 1–3 ✓ |

---

## 5. Konvensi

- **Bahasa C++**: C++17, namespace root `kairo::`. Header di `firmware/core/include/kairo/...`, impl di `firmware/core/src/...`.
- **Hardware-agnostic**: file di `firmware/core/**` **dilarang** `#include` apapun yang spesifik platform (no `<Arduino.h>`, no ESP-IDF, no `Bun`, dst). Core hanya kenal interface abstrak.
- **No `printf` langsung** — selalu lewat Logger (master plan §7).
- **Capability-driven** — cek `capabilities.has("wifi")`, bukan `isEsp32()` (master plan §3).
- **Penamaan dok**: `NN-slug.md`, dua digit, berurutan.
- **Definition of Done** tiap stage: kode kompilasi, ada cara verifikasi yang dijelaskan, dan checklist `Tasks` tercentang.

## 6. Glosarium singkat

- **Core** — runtime portable, tak kenal hardware.
- **Platform** — implementasi environment (simulator/esp32). MVP: simulator.
- **Board** — definisi hardware (virtual untuk MVP).
- **Target** — entry point firmware (`main()`), merakit Platform+Board+Core.
- **Driver** — implementasi konkret sebuah hardware (dummy untuk MVP).
- **Bridge** — jembatan stdio↔WebSocket antara binary C++ dan web UI.
