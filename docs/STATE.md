# Palanu — Current State

> Living snapshot keadaan proyek. Baca ini dulu sebelum lanjut kerja.
> Detail per-stage ada di [`plans/`](plans/00-overview.md). Master plan: [`concept_plan.md`](concept_plan.md).
> Reference arsitektur per-subsistem: [`architecture/`](architecture/README.md).
>
> **Last updated:** 2026-06-21 (Plan 82 + UI polish — canvas scaling, PS launcher, status bar fixes, VirtualKeyboard redesign + 2D nav fix)

---

## TL;DR

Palanu = platform handheld bergaya Flipper Zero, **1-bit retro/pixel UI**, dengan Core Runtime C++ portable yang jalan di **simulator** (web) dan **hardware ESP32-S3 + e-ink**.

**Status: MVP + App Registry (Flipper-style apps/services, eks-Plugin) + UI Runtime + ESP32 dev board + Nema kernel (multi-thread) + App-model + WiFi + HTTP + Virtual Keyboard + networked apps.** Firmware **berhasil di-flash & jalan di device fisik** sampai Fase B app-model (dikonfirmasi developer). Fitur konektivitas (WiFi/Ticker/keyboard) sudah build dual-target + terverifikasi di simulator; **belum diverifikasi di hardware**.

---

## Apa yang sudah jalan

| Area | Status | Bukti |
|---|---|---|
| **Asset architecture (Plan 82)** | ✅ build (host+wasm) | T1 system icons (status bar), T2 launcher icon anims, T3 `.panim` (VFS); toolchain `tools/asset_gen/`; `dolphin_showcase.cpp` 895 KB removed; BadUSB → category="System"; battery icon 16×8 (proportional); WiFi icon state-gated (`available()`); animation consolidated to `boxing.panim`; LittleFS 512 KB |
| **Canvas scaling (non-integer)** | ✅ build | `fillRect`/`invertRect`/`drawPixel` pakai floor-edge formula — tidak ada gap/double-invert artifact di scale 1.75× / 1.5× |
| **PlayStation launcher** | ✅ build | Flush-left pada layar sempit (≤3 tile); partial tile peek di kanan sebagai scroll hint |
| **Aether display server = lib terpisah (Plan 80)** | ✅ build (host+wasm+esp32) | `nema_core` 0 ref ke `aether` (IDF strict-link); semua UI/screens/GuiService → `libaether`; ganti server = ganti lib + `aether::bootDisplay(rt)` |
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
| **Virtual Keyboard** (Flipper-style compact, capped width, rounded fill selection, no borders; 4 modes: ABC/abc/123/!@#; 2D nav fixed on WASM + hardware) | ✅ build | sim render ✓ |
| **HTTP client HAL** (sim=curl, esp32=esp_http_client+TLS) | ✅ build | sim live Binance 200 |
| **Ticker app** (BTC/USD via Binance, fetch di worker, UI tak freeze) | ✅ build | sim ✓ (HW pending) |
| **Sim WiFi "router"** interaktif (network list, password, RSSI, online toggle) | ✅ | web panel + 4-skenario ✓ |
| **Secure element HAL** (`ISecureElement`, `caps::Secure`; backend SE050 skyrizz + sim) | 🟡 scaffold | build host+wasm+esp32 ✓; ops crypto TODO (ADR 0005) — fondasi crypto wallet |

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
Desktop (live wallpaper, idle) — Plan 81
 └── OK → Launcher (skin: PlayStation carousel | Nintendo Wii grid)
       ├── Apps → AppRegistry.list() → launch app (thread sendiri via AppHost)
       │     Clock · Counter (+modal) · Stopwatch (fullscreen) · Task Demo · Ticker
       ├── Files · Dolphin · Logs
       ├── Settings → Display & Appearances (Theme/Desktop/Launcher/AssetsPack/StatusBar)
       │     · WiFi · About (board/fw/caps)
       └── System: BadUSB (category="System", launched via launcher System section)
```

> Shell skins are swappable via `nema::shell` (Plan 81 / ADR 0004): Desktop and
> Launcher each pick a skin from config (`display/desktop`, `display/launcher`).
> See [`feats/shell-desktop-launcher.md`](feats/shell-desktop-launcher.md).

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
- **✅ Regresi UI WiFi/BLE DIPERBAIKI (2026-06-20, plan 72-73):** rewrite UI/Aether (commit `1230296`) sempat menghapus UI WiFi/BLE. Sekarang dibangun ulang sebagai `WifiSettingsScreen` + `BluetoothSettingsScreen` (ComponentScreen, digate caps) di Settings. **Build hijau host+wasm+esp32.** Juga ditemukan **BLE mati di sdkconfig skyrizz** (`CONFIG_BT_ENABLED` tak diset → esp32_ble = stub) — sudah diaktifkan (NimBLE + WiFi/BLE coexistence). Tinggal flash & HW-verify.

---

## Seri aktif: Connectivity & Network MVP (plan 72–78)

> Fokus berikutnya yang dikunci: matangkan konektivitas → remote-over-network ber-auth →
> deploy app/daemon → Forge CLI. Semua sudah diplan-detailkan (teknis + acceptance).
> **Eksekusi berantai** — tiap plan menunggu yang sebelumnya:

1. **[72](plans/72-connectivity-hal-maturation.md)** — ✅ **CODE DONE (build host+wasm+esp32)**: WiFi state machine + rssi + saved-networks + liveness + WifiStateChanged + country code; BLE liveness. _(extras: IDL/contract-test/doc ditunda)_
2. **[73](plans/73-connectivity-settings-ui.md)** — ✅ **CODE DONE (build 3 target)**: `WifiSettingsScreen` + `BluetoothSettingsScreen` di Settings (digate caps). **Fix kritis: BLE diaktifkan di sdkconfig skyrizz** (NimBLE + coexistence) — sebelumnya stub. _HW-verify = user._
3. **[74](plans/74-remote-access-auth.md)** — 🔴 **DITUNDA** (kode keamanan tak diburu): tier channel + Settings→Remote + auth password + BLE bond-as-cache. _Goal konek WiFi/BLE sudah jalan tanpa auth di observation tier._
4. **[75](plans/75-network-link-transport.md)** — ✅ **CODE DONE (build skyrizz + forge typecheck)**: `Esp32WsTransport` (WS `/plp@8477`) + mux + lifecycle gating + mDNS; Forge web `WebSocketTransport` + opsi "Network (Wi-Fi)" di `/remote`. → **Forge web bisa remote via WiFi**. _HW-verify = user._
5. **[76](plans/76-app-service-daemon-model.md)** — Model App vs Service/daemon headless + autostart + deploy (pakai persist [38](plans/38-storage-filesystem-hal.md)). _(nunggu 38, 74)_
6. **[77](plans/77-palanu-link-shared-lib.md)** — `@palanu/link`: ekstrak codec+session+auth shared (Forge web ⇄ CLI). _(nunggu 74)_
7. **[78](plans/78-forge-cli.md)** — Forge CLI (`palanu`): cli/logs/deploy/ota/fs over USB/BLE/TCP. _(nunggu 77, 75, 76)_

> Persistence app = **plan 38** (sudah ada, dipakai oleh 76). Urutan kritis-path:
> 72→73→74→75 (network), lalu 76+77 paralel, lalu 78.

---

## Kandidat kerja lain (di luar seri aktif)

- **Plan 21** (screen sleep + lock) — UX polish.
- **Plan 22** (app pause/resume) — butuh 19.6 Fase C/D tuntas.
- **Fase 19.6 D**: per-core tuning + crash isolation demo.
- WiFi: static IP apply (esp_netif) — sebagian masuk plan 72 (saved networks).
- **Palanu Board V1**: desain PCB + board layer (reuse platform esp32).
- **Utang doc:** `overview.md`/`STATE.md` belum mencerminkan Aether/IDL/app-runtime; **rename KLP→PLP** belum tuntas di docs (kode sudah PLP). Selesaikan sebelum codebase membesar.
