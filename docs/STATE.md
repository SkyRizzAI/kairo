# Palanu — Current State

> Living snapshot keadaan proyek. Baca ini dulu sebelum lanjut kerja.
> Detail per-stage ada di [`plans/`](plans/00-overview.md). Master plan: [`concept_plan.md`](concept_plan.md).
> Reference arsitektur per-subsistem: [`architecture/`](architecture/README.md).
>
> **Last updated:** 2026-06-20

---

## TL;DR

Palanu = platform handheld bergaya Flipper Zero, **1-bit retro/pixel UI**, dengan Core Runtime C++ portable yang jalan di **simulator** (web) dan **hardware ESP32-S3 + e-ink**.

**Status: MVP + App Registry (Flipper-style apps/services, eks-Plugin) + UI Runtime + ESP32 dev board + Nema kernel (multi-thread) + App-model + WiFi + HTTP + Virtual Keyboard + networked apps.** Firmware **berhasil di-flash & jalan di device fisik** sampai Fase B app-model (dikonfirmasi developer). Fitur konektivitas (WiFi/Ticker/keyboard) sudah build dual-target + terverifikasi di simulator; **belum diverifikasi di hardware**.

---

## Apa yang sudah jalan

| Area | Status | Bukti |
|---|---|---|
| Core Runtime (boot, logger, event bus, services, introspection) | ✅ HW | jalan di sim + esp32 |
| App Registry (AppManifest, AppRegistry — built-in/custom apps + services; menggantikan Plugin Runtime) | ✅ HW | install/list/launch |
| UI Runtime retro (Canvas 1-bit, font 5×8, ViewDispatcher) | ✅ HW | semua screen render |
| Palanu Dev Board (ESP32-S3 + e-ink GxEPD2 + 6 tombol TCA9534) | ✅ HW | build + flash + jalan |
| **Async display** (e-ink flush di task terpisah, dirty-rect, latest-wins) | ✅ HW | tombol tak freeze saat refresh |
| **Nema kernel** (`nema::Thread`, `MessageQueue`, `TaskRunner`) | ✅ HW | F0–1 + TaskRunner di board |
| **Input thread** (TCA9534 poll di thread sendiri → InputService) | ✅ HW | fix "pencet hilang/loncat" |
| **GuiService thread** (render + input dispatch lepas dari main loop) | ✅ HW | flashed Fase A |
| **App-model** (`IApp`/`AppHost`/`AppContext`, app = thread sendiri) | ✅ HW | Counter app-thread di board |
| Semua app → app-model (Clock/Counter/Stopwatch/TaskDemo) | ✅ build | sim ✓, status bar in-app |
| **WiFi UI** (WifiApp: scan/pick/password/connect, non-blocking) | ✅ build | sim ✓ (HW pending) |
| **Virtual Keyboard** (QWERTY + CAPS + 123/sym + DEL/SPACE/OK/ESC + password mode) | ✅ build | sim render ✓ |
| **HTTP client HAL** (sim=curl, esp32=esp_http_client+TLS) | ✅ build | sim live Binance 200 |
| **Ticker app** (BTC/USD via Binance, fetch di worker, UI tak freeze) | ✅ build | sim ✓ (HW pending) |
| **Sim WiFi "router"** interaktif (network list, password, RSSI, online toggle) | ✅ | web panel + 4-skenario ✓ |

**Pilar arsitektur "tidak pernah freeze" terbukti:** scan WiFi (1-3s) dan HTTP fetch (1-3s) jalan di `TaskRunner` worker thread sementara UI tetap render & responsif. Reference firmware-nya sendiri komentar "freezes UI during fetch" — Palanu tidak.

---

## Arsitektur thread (Nema kernel)

```
core 1 (UI)                  core 0 (work/IO)            main loop (Arduino)
──────────                   ────────────────            ──────────────────
GuiService thread            TaskRunner worker           rt.step():
 owns Canvas+ViewDispatcher   scan(), http.get()          asyncPoster.flush()
 input → handleKey            (boleh BLOCK)               serviceManager.tick()
 screen draw → flush
 task completions            Input poll thread (TCA9534)  platform.idle()
                              → InputService queue
 App thread (foreground)
  IApp::run() — may BLOCK; draws to own buffer; present() → GUI thread
```

**Race-free by design:** state lintas-thread cuma pixel buffer (mutex) + queue. Nol shared model. App tak pernah sentuh Canvas/ViewDispatcher langsung.

Nama kernel: **Nema** (νῆμα = "benang/thread"), namespace `nema::nema`. "Furi" hanya istilah pembanding Flipper.

---

## Navigasi UI

```
Home (Apps / Logs / Settings)
 ├── Apps → AppRegistry.list() → launch app (thread sendiri via AppHost)
 │     Clock · Counter (+modal) · Stopwatch (fullscreen) · Task Demo · Ticker
 ├── Logs → uptime + jumlah app
 └── Settings → WiFi (→ WifiApp) · About (board/fw/caps)
```

---

## Tier hardware (penamaan resmi)

1. **Simulator** (`board=simulator`) — virtual, host/web. Dummy driver + "router" WiFi sim.
2. **Palanu Dev Board** (`board=dev-board`) — ESP32-S3-WROOM-1 + e-ink 264×176 + TCA9534. **← sekarang di sini.**
3. **Palanu Board V1** (`board=palanu-board-v1`) — PCB custom. **Belum didesain.**

---

## Cara build & run

```bash
# Simulator (web UI) — firmware compiled to WASM, runs in the browser
bun install
bun run forge:wasm     # build core C++ → WASM → Forge (/simulator)
bun run test           # host unit tests (layout/KLP/link) via ctest

# Palanu Dev Board (ESP32-S3) — ESP-IDF v5.5 di ~/esp/esp-idf
bun run build:esp32    # → build/palanu-dev-board.bin (~1.3 MB, 59% free)
bun run flash:esp32    # flash + serial monitor
```

---

## Peta repo (tambahan dari sesi konektivitas)

```
firmware/core/include/palanu/
  nema/        thread.h, message_queue.h, task_runner.h, input_event.h   # kernel
  app/         app.h, app_context.h, app_host.h                          # app-model
  apps/        counter_app, clock_app, stopwatch_app, task_demo_app,
               wifi_app, ticker_app                                      # app implementations
  services/    input_service.h, gui_service.h                           # threaded services
  hal/         http_client.h, async_display.h, buffer_display.h, wifi.h
  ui/          virtual_keyboard.h, text_input.h, components.h (drawTitle/drawConfirm)
firmware/core/src/nema/  thread_esp32.cpp | thread_host.cpp  (conditional per build)
platforms/wasm/          wasm_platform, wasm_cable_transport (KLP), sim_wifi_driver ("router")
platforms/esp32/         esp32_http_client (esp_http_client+TLS), esp32_wifi_driver (scan/NVS)
packages/forge/          SvelteKit web client: /simulator (WASM), /remote, /flash
```

---

## Keputusan arsitektur (dikunci, tambahan)

- **Tidak ada blocking di UI**: kerja berat → `TaskRunner::submit(work, done)`. `work` di worker thread (boleh block), `done` di UI thread (aman sentuh state).
- **Driver tak pegang FreeRTOS langsung** kecuali low-level: pakai abstraksi (`AsyncEventPoster`, `AsyncDisplayDriver`, `nema::Thread`).
- **App = thread sendiri**, gambar ke buffer, `present()` handoff ke GUI thread. Status bar di-composite GUI thread (mode Normal) atau app penuh (Fullscreen).
- **HTTP gated ke status WiFi** (di sim juga, lewat `isOnline()`) supaya jujur seperti hardware.
- **Lifecycle driver** via `IDriver::onRegister(rt)` — self-register deps/service/caps/hw.

### Recent decisions (ADR)

> Full records in [`decisions/`](decisions/). One file per decision, append-only.

- [0001](decisions/0001-usb-jtag-remote-uses-hwcdc.md) — Forge remote over USB
  Serial/JTAG drives HWCDC directly (not Arduino `Serial`); branch on the **value** of
  `ARDUINO_USB_CDC_ON_BOOT` with `#if`, never `#ifdef`.

---

## Catatan / gap yang diketahui

- **Belum diverifikasi HW**: WiFi UI, Ticker, Virtual Keyboard, app-model Fase C (semua app). Build dual-target ✅, sim ✅, tinggal flash & tes.
- **Fase 19.6 C/D belum tuntas di HW**: GuiService + app-model sudah di board sampai Fase B (Counter). Migrasi semua app (C) baru terverifikasi build/sim.
- **TLS ESP32**: pakai `esp_crt_bundle_attach`. `insecure` flag = skip common-name check.
- **api.binance.com** 404 di region dev → pakai `data-api.binance.vision` (mirror publik, sama seperti ref firmware).
- **Web panel tsconfig** kurang `lib:["dom"]` → tsc lapor error `window`/`document` (pre-existing, bukan bug; Bun bundler tetap jalan).
- **Persistence flaky** selama sesi konektivitas: beberapa file simulator/web berkali revert; semua sudah di-verifikasi di disk + re-apply.
- Dev Board tak punya battery monitoring (no ADC). Built-in apps masih di-install static di `main.cpp` (custom apps: OTA via KLP; microSD menunggu Plan 38).

---

## Kandidat kerja berikutnya

- **Flash & verifikasi HW**: WiFi end-to-end (scan→keyboard→connect), Ticker over real WiFi, keyboard di e-ink.
- **Plan 21** (screen sleep + lock) — UX polish.
- **Plan 22** (app pause/resume) — butuh 19.6 Fase C/D tuntas.
- **Fase 19.6 D**: per-core tuning + crash isolation demo.
- WiFi: static IP apply (esp_netif), multiple saved networks (NVS profiles).
- **Palanu Board V1**: desain PCB + board layer (reuse platform esp32).
