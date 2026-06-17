# Palanu — MVP Roadmap

> Apa yang harus selesai sebelum Palanu bisa di-claim sebagai MVP yang layak
> dipakai orang lain. Diurutkan per dampak vs effort.
>
> **Last updated:** 2026-06-16
> **Reference:** `docs/research/competitive-analysis.md`, `docs/STATE.md`

---

## Definisi MVP Palanu

MVP = **device bisa diberikan ke orang lain dan mereka tidak frustrasi dalam 10 menit pertama.**

Kriteria konkret:
- [ ] Apps tidak hilang setelah reboot
- [ ] Jam menunjukkan waktu nyata setelah connect WiFi
- [ ] WiFi connect, HTTP fetch bekerja di hardware
- [ ] Tidak ada blank screen / freeze yang tidak bisa di-recover
- [ ] Minimal 8 built-in apps yang berfungsi
- [ ] Satu fitur "wow" yang tidak ada di Flipper stock
- [ ] Bisa install app baru dari Forge tanpa compile ulang firmware
- [ ] Forge: simulator, flash, install app semua jalan

---

## Status Sekarang (2026-06-16)

### Yang Sudah Berjalan ✅

| Area | Keterangan |
|------|-----------|
| Core Runtime (boot, logger, event bus) | HW confirmed |
| UI system (flex layout, widgets, renderer) | HW + sim |
| ComponentApp / AppHost / AppRegistry | HW + sim |
| QuickJS JS apps (Counter, SysInfo embed) | Sim ✅; HW intermittent |
| KLP protocol (BLE + USB transport) | Build verified |
| Forge (simulator + remote + flash) | Running |
| DPM (sleep/lock timeout) | HW |
| Profile (name/password) | HW |
| Settings screens (display, sleep, about) | HW |
| App pause/resume | HW |
| Virtual keyboard | Sim + build |
| App install via KLP (volatile) | Sim + build |

### Gap Kritis (blocker) ❌

| Gap | Severity | Notes |
|-----|----------|-------|
| **App hilang setelah reboot** | P0 | Plan 38 — LittleFS belum |
| **JS apps blank di SkyRizz E32** | P0 | Intermittent; root cause unknown |
| **Jam tidak tahu waktu nyata** | P1 | NTP belum ada |
| **WiFi belum diverifikasi di HW** | P1 | Build ✅, sim ✅, HW: belum ditest |
| **Battery monitoring** | P2 | Tidak ada di dev-board; SkyRizz E32 perlu driver |

---

## Backlog MVP (diurutkan prioritas)

### P0 — Fix sekarang, semua hal lain block ini

---

#### [P0-1] Fix JS App Blank Screen (SkyRizz E32)

**Status:** Nz > 0 dikonfirmasi di `present()` (Counter nz=199, SysInfo nz=3301),
tapi tampilan intermittent. Hello app sudah fixed (stub → ComponentApp).

**Hipotesis yang belum dicek:**
- Timing: `draw()` pertama dipanggil sebelum `present()` pertama. hasFrame=false
  → draw skip. Setelah itu tidak ada redraw trigger.
- DPM: sleep/lock masuk sebelum frame pertama render.
- `flushBuffer` vs `flush` path: cek apakah ada kondisi di mana LcdDriver tidak
  benar-benar flush ke panel.

**Investigasi:**
```
Log yang perlu dicek:
AppHost draw n=0 hasFrame=0  ← draw sebelum present
AppHost draw n=1 hasFrame=0
AppHost present n=0 nz=199   ← present setelah draw sudah lewat
AppHost draw n=2 hasFrame=1  ← baru bisa draw

Apakah setelah draw n=2, GUI thread flush ke display?
Cek: tambahkan log di GuiService::loop() setelah server_->renderFrame() + canvas.flush()
```

**Fix kandidat:**
- `AppHost::present()` sudah panggil `rt_.view().requestRedraw()`. Pastikan ini
  sampai ke GuiService dan loop picks it up sebelum sleep 5ms.
- Atau: di `AppHost::tick()`, sudah ada safety net `if (frameSeq_ != drawnSeq_) requestRedraw()` — pastikan ini firing.

---

#### [P0-2] Persistent App Storage — Plan 38

**Status:** `JsAppStore::installKapp()` bisa install ke RAM tapi hilang setelah reboot.
`loadInstalledPapps()` scan `/apps/` sudah ada — tinggal mount nyata.

**Implementasi:**

1. **Mount LittleFS di `/internal/`** pada boot (platform esp32):
   ```cpp
   // platforms/esp32/src/esp32_runtime.cpp
   auto lfs = std::make_shared<LittleFsFileSystem>("/spiffs");
   rt.fs().mount("/internal", lfs);
   ```

2. **Simpan `.kapp` ke `/internal/apps/<id>.kapp`** saat install:
   ```cpp
   // core/src/apps/js_app_store.cpp — installKapp()
   // Setelah register ke AppRegistry, tulis ke VFS:
   if (auto* fs = rt_.fs().open("/internal/apps/" + id + ".kapp", "w"))
       fs->write(bundle);
   ```

3. **Load on boot** sudah ada — `loadInstalledPapps()` scan `/apps/` → tinggal
   pastikan `/internal/apps/` di-mount sebelum ini dipanggil.

4. **Partisi flash**: di `partitions.csv` target SkyRizz E32, pastikan ada partisi
   `spiffs` cukup besar (≥512KB untuk beberapa app bundle).

**Files:**
- `firmware/platforms/esp32/` — tambah LittleFS mount
- `firmware/core/src/apps/js_app_store.cpp` — tambah write on install
- `firmware/targets/skyrizz-e32/partitions.csv` — cek/tambah spiffs partition

**Effort:** M (2–3 hari) | **Dampak:** P0, kritis untuk semua user flow

---

### P1 — Must have sebelum claim MVP

---

#### [P1-1] NTP Time Sync

**Status:** `ClockService` ada. Status bar punya slot jam. Tapi tidak ada NTP —
jam selalu mulai dari Unix epoch (atau waktu compile).

**Implementasi:**

```cpp
// core/src/services/ntp_service.cpp (baru)
class NtpService : public IService {
    void start() override {
        rt_.events().subscribe(events::NetworkConnected, [this](const Event&) {
            rt_.tasks().submit([this] { syncOnce(); }, [](auto) {});
        });
    }
    void syncOnce() {
        // ESP-IDF: esp_sntp_init() + sntp_setservername() + sntp_restart()
        // atau: UDP socket ke pool.ntp.org:123, parse NTP packet manual
        // Set: rt_.clock().setEpochMs(ntpResult);
    }
};
```

- Add `NtpService` ke service container di `runtime.cpp`
- Tampilkan real time di StatusBar setelah sync (sudah ada `status_.hour/minute`)

**Effort:** S (1 hari) | **Dampak:** Tinggi — jam yang salah terlihat tidak profesional

---

#### [P1-2] WiFi Hardware Verification

**Status:** WiFi driver (`Esp32WifiDriver`) + HTTP client (`Esp32HttpClient`) ada.
Sim ✅. Hardware: **belum pernah ditest end-to-end**.

**Test yang harus dilakukan:**
1. WifiApp: scan → pick SSID → masukkan password via keyboard → connect
2. Setelah connect: cek `NetworkConnected` event + `isOnline() == true`
3. Ticker app: HTTP GET ke Binance, tampilkan harga
4. NTP: sync jam setelah connect

**Kemungkinan bug di HW yang belum ditemukan:**
- TLS cert bundle tidak ter-embed: periksa `idf_component.yml` include `esp_crt_bundle`
- WiFi country code: set `"ID"` atau `"US"` supaya channel 1–13 bisa discan
- Password WiFi tidak ter-persist ke NVS (config store sudah ada, cek flow save)

**Files:** `boards/skyrizz-e32/src/`, `platforms/esp32/src/esp32_wifi_driver.cpp`

---

#### [P1-3] App Crash Recovery

**Status:** Kalau JS app hang atau throw unhandled, app thread stall. Tidak ada
auto-restart.

**Implementasi:**

```cpp
// core/src/app/app_host.cpp — threadEntry()
void AppHost::threadEntry(void* self) {
    auto* h = static_cast<AppHost*>(self);
    try {
        h->app_.run(*h);
    } catch (const std::exception& e) {
        h->rt_.log().error("AppHost", "app crashed", {{"app", h->app_.name()}, {"what", e.what()}});
    } catch (...) {
        h->rt_.log().error("AppHost", "app crashed (unknown)");
    }
    // thread exits → AppHost::tick() sees !thread_.running() → pop screen
}
```

Untuk restart policy (AkiraOS-style):
```cpp
// AppHostManager: track crash_count per app_id
// Setelah pop(), jika crash_count < 3: re-push after backoff (1s, 5s, 30s)
// Setelah 3x: mark FAILED, tampilkan error screen, jangan restart lagi
```

**Effort:** S–M | **Dampak:** Production stability

---

#### [P1-4] Battery Monitoring (SkyRizz E32)

**Status:** Dev board tidak punya battery ADC. SkyRizz E32 perlu dicek schematic.

**Opsi:**
a. Baca ADC GPIO di SkyRizz E32 jika tersedia (cek board_config.h)
b. Dummy driver yang estimasi dari waktu uptime (tidak akurat tapi UI lebih baik)
c. Expose via XL9535 GPIO expander jika ada fuel gauge IC

**Quick fix**: tambah `DummyBatteryDriver` yang return hardcoded "85%" tapi
memunculkan icon di status bar — lebih baik dari blank.

**Effort:** S | **Dampak:** UX polish

---

### P2 — Fitur penting sebelum public release

---

#### [P2-1] USB HID + DuckyScript BadUSB App

Ini adalah fitur yang paling mudah diimplementasi dengan dampak terbesar.

**Kenapa ini prioritas:**
- TinyUSB sudah ada (CDC-ACM class)
- DuckyScript minimal: `STRING`, `DELAY`, `ENTER`, `CTRL+`, `GUI+`, `ALT+`
- Diferensiasi vs Flipper stock: **JS scripting untuk BadUSB** — user bisa nulis
  payload `.js` bukan hanya `.txt` DuckyScript, jauh lebih powerful

**Implementasi:**

1. **USB HID class** di TinyUSB composite device:
   ```c
   // platforms/esp32/include/tusb_config.h
   #define CFG_TUD_HID 1
   ```
   Add keyboard HID descriptor + report.

2. **`IUsbHid` interface**:
   ```cpp
   class IUsbHid {
   public:
       virtual void sendKey(uint8_t modifier, uint8_t keycode) = 0;
       virtual void sendString(const char* s, uint32_t delayMs = 0) = 0;
       virtual void releaseAll() = 0;
   };
   ```

3. **`BadUsbApp`**: load script dari `/badusb/<name>.duck`, parse + execute.
   ```
   STRING Hello World
   ENTER
   DELAY 1000
   CTRL ALT t
   STRING ls -la
   ENTER
   ```

4. **JS BadUSB API**: expose ke nema JS:
   ```js
   import { badusb } from "nema";
   await badusb.type("Hello World\n");
   await badusb.combo("CTRL", "ALT", "t");
   ```

**Files baru:** `hal/usb_hid.h`, `platforms/esp32/src/esp32_usb_hid.cpp`,
`core/src/apps/bad_usb_app.cpp`

**Effort:** M (3–4 hari) | **Dampak:** High — security community killer feature

---

#### [P2-2] BLE Scanner App

**Status:** BLE peripheral (advertising + GATT server) sudah ada. Central mode
(scanning) belum.

**Implementasi:**

```cpp
// hal/ble_scanner.h
class IBleScanner {
public:
    struct Device { std::string mac, name; int8_t rssi; std::vector<uint16_t> services; };
    virtual void startScan(uint32_t durationMs, std::function<void(Device)> onFound) = 0;
    virtual void stopScan() = 0;
};
```

- `Esp32BleScanner`: NimBLE `ble_gap_disc()` sudah ada di NimBLE API
- `BtScanApp`: scrollable list (MAC, name, RSSI), auto-refresh setiap 5 detik

**JS API**: `nema.ble.scan(durationMs)` → array of devices

**Effort:** M | **Dampak:** Tinggi — BLE hacking feature

---

#### [P2-3] Capability Enforcement (Manifest-based)

**Status:** Saat ini `nema.http.*` tersedia ke semua JS apps tanpa filter.

**Implementasi:**

`.kapp` manifest sudah punya field untuk capabilities. Saat `JsApp::onStart()`:
```cpp
void JsApp::onStart(AppContext& ctx) {
    // Parse manifest capabilities[]
    auto caps = manifest_.capabilities; // ["net.wifi", "storage"]

    // Install hanya bridge yang diizinkan
    if (hasCap(caps, "net.wifi"))    installHttpBridge(eng_);
    if (hasCap(caps, "storage"))     installStorageBridge(eng_);
    if (hasCap(caps, "device.ble"))  installBleBridge(eng_);
    // dst.
}
```

Ini juga membuat manifest `.kapp` terlihat di Forge sebelum install — user tahu
app minta permission apa.

**Effort:** M | **Dampak:** Fondasi keamanan

---

#### [P2-4] Secure Boot + OTA Signing

**Status:** OTA via KLP ada. Tapi firmware ditulis raw tanpa verifikasi.

**Implementasi:**
- ESP-IDF Secure Boot V2: RSA-PSS 3072-bit signature
- Forge: build pipeline sign firmware sebelum flash
- `Esp32OtaUpdater::commit()`: verifikasi signature sebelum mark valid
- Rollback: sudah punya `confirmBoot()` tinggal wire ke health check setelah boot

**Effort:** L | **Dampak:** Production critical

---

### P3 — Post-MVP differentiators

---

#### [P3-1] Color UI via LVGL (SkyRizz E32)

SkyRizz E32 punya 240×320 IPS TFT — sayang kalau hanya 1-bit. LVGL bisa
jalan di ESP32-S3 dengan DMA-accelerated SPI flush.

**Arsitektur:**
- `LvglServer` implements `IDisplayServer`
- Saat user `display start lvgl` atau app set `fullscreen() + colorMode()`:
  LVGL render ke `framebuf` RGB565, flush via `LcdDriver::flushRgb565()`
- Aether (1-bit) tetap default — LVGL opt-in per app

**Effort:** L (1–2 minggu) | **Dampak:** Visual differentiator

---

#### [P3-2] AI Inference App (Camera + MobileNet/TFLite)

SkyRizz E32 punya kamera GC2145 + ESP32-S3 punya hardware acceleration
(vector instructions). Tidak ada platform lain yang bisa ini.

**Proof of concept:**
- Capture frame dari GC2145
- Run TFLite Micro (bisa di-pull sebagai ESP-IDF component)
- Klasifikasi objek sederhana (person/no person, gesture)
- Tampilkan hasil di UI

**Dampak strategis:** "AI-powered hacker toy" — positioning yang benar-benar unik.

**Effort:** L | **Dampak:** Strategis / wow factor

---

#### [P3-3] Forge App Store

- Hosted JSON registry: `{ apps: [{ id, name, version, description, url }] }`
- `AppStoreApp` (on-device): browse catalog, download + install dari device
- Forge `/apps` page: submit app, browse, install ke connected device via KLP

**Effort:** L (butuh backend + community) | **Dampak:** Jangka panjang sangat tinggi

---

## Summary Table

| ID | Feature | Priority | Effort | Status |
|----|---------|----------|--------|--------|
| P0-1 | Fix JS blank screen | P0 | S–M | 🔴 Bug |
| P0-2 | Persistent storage (LittleFS) | P0 | M | 🔴 Not started |
| P1-1 | NTP time sync | P1 | S | 🔴 Not started |
| P1-2 | WiFi hardware verification | P1 | S | 🟡 Test needed |
| P1-3 | App crash recovery | P1 | S–M | 🔴 Not started |
| P1-4 | Battery monitoring | P1 | S | 🟡 Partial |
| P2-1 | USB HID + BadUSB app | P2 | M | 🔴 Not started |
| P2-2 | BLE scanner app | P2 | M | 🔴 Not started |
| P2-3 | Capability enforcement | P2 | M | 🔴 Not started |
| P2-4 | Secure boot + OTA signing | P2 | L | 🔴 Not started |
| P3-1 | LVGL color UI | P3 | L | 🔴 Not started |
| P3-2 | AI inference (camera) | P3 | L | 🔴 Not started |
| P3-3 | Forge app store | P3 | L | 🔴 Not started |

---

## Catatan Arsitektur (jangan diubah)

Ini yang sudah baik dan tidak perlu diganti untuk MVP:
- Core/platform/board/target layering — pertahankan
- ComponentApp + flex layout — jangan ganti dengan LVGL untuk native screens
- KLP protocol — sudah lengkap, tidak perlu redesign
- QuickJS sebagai runtime JS utama — WAMR bisa nanti sebagai opsional
- Forge WASM simulator — USP terbesar kita vs semua kompetitor

---

## Estimasi Timeline MVP

Asumsi: 1 developer penuh, firmware only.

| Phase | Items | Estimasi |
|-------|-------|----------|
| **Phase 0** — Hardware stable | P0-1, P0-2 | 1–2 minggu |
| **Phase 1** — Feature complete MVP | P1-1, P1-2, P1-3, P1-4 | 2–3 minggu |
| **Phase 2** — Differentiators | P2-1, P2-2, P2-3 | 3–4 minggu |
| **Phase 3** — Public release prep | P2-4, docs, Forge polish | 2–3 minggu |

**Total estimasi ke MVP:** ~8–12 minggu dari sekarang.

MVP tidak butuh sub-GHz/NFC (butuh hardware rev), tidak butuh LVGL (nice to have),
tidak butuh AI (post-MVP wow factor). Fokus: **stabil, persistent, reliable, 1 killer feature**.
