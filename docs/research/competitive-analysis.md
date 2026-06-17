# Palanu — Competitive Analysis & Gap Assessment

> Perbandingan Palanu terhadap platform sejenis: Flipper Zero + Momentum,
> AkiraOS, Hak5, M5Stack, PineTime, dan alat-alat "geek toy" lain. Tujuan:
> tahu posisi kita, gap nyata vs MVP, dan mana yang layak diadopsi.
>
> **Last updated:** 2026-06-16

---

## TL;DR

Palanu unggul di **developer ergonomics** (WASM sim, JS apps, modern UI, KLP
remote) tapi masih sangat tertinggal di **hardware breadth**, **app ecosystem**,
dan **production stability**. Flipper Zero adalah benchmark paling relevan (bukan
Hak5 — beda target). Kita butuh: persistent storage, hardware stability, 10+
useful apps, dan 1 "killer feature" yang bikin orang pilih Palanu bukan Flipper.

---

## 1. Platform yang Dibandingkan

| Platform | Target | OS/Runtime | Form Factor | Notes |
|----------|--------|------------|-------------|-------|
| **Flipper Zero** (stock) | Hacker/hobbyist | FuriOS (FreeRTOS) + C FAP apps | Handheld, 128×64 1-bit | 2M+ unit dijual |
| **Momentum** (Flipper FW) | Power user Flipper | Fork FuriOS + more protocols | Same hardware | Custom FW paling populer |
| **AkiraOS** | Embedded dinamis | Zephyr + WAMR | Variatif | Sudah dikaji di `akiraos-vs-kairo.md` |
| **Hak5 WiFi Pineapple** | Pentester WiFi | OpenWRT + module system | Router form | Bukan handheld; beda target |
| **Hak5 Bash Bunny / O.MG** | RedTeam | Custom Linux | USB dongle | Ultra-specialized |
| **M5Stack** | Maker/IoT | Arduino + UIFlow | Modular box | Color display, banyak modul |
| **Badger 2040 W** | Badge/education | MicroPython | Badge e-ink | Sangat simple |
| **PineTime + InfiniTime** | Smartwatch OS | Embedded C + BLE | Wristwatch | Beda form, tapi UI+OTA bagus |
| **Meshtastic** | LoRa mesh | ESP32 | Variatif | Focused networking, bukan UI |
| **PortaPack H2 (HackRF)** | SDR + UI | Custom C++ | HackRF addon | SDR-specific UI |

---

## 2. Flipper Zero + Momentum — Benchmark Paling Relevan

Ini perbandingan yang paling penting karena target market dan form factor paling mirip.

### 2.1 Apa yang Flipper Lakukan dengan Baik

#### Hardware Protocol Support (Palanu: ❌ Nihil)
| Fitur Flipper | Chip | Status di Palanu |
|---------------|------|-----------------|
| Sub-GHz radio | CC1101, 300–928 MHz | ❌ Tidak ada |
| NFC reader/writer | STM32, ISO 14443/15693 | ❌ Tidak ada |
| RFID 125kHz | EM4100, HID, Indala | ❌ Tidak ada |
| Infrared TX/RX | TSOP + IR LED | ❌ Tidak ada (SkyRizz punya kamera tapi bukan IR TX) |
| iButton / 1-Wire | DS1990A | ❌ Tidak ada |
| BadUSB / HID | TinyUSB HID class | ❌ Ada TinyUSB tapi hanya CDC-ACM, bukan HID |
| GPIO header | 8 pin + UART/SPI/I2C/1-Wire | ❌ Tidak ada ekspansi eksternal |

#### Dynamic App Ecosystem (Palanu: 🚧 Parsial)
- **Flipper** punya 1000+ FAP apps (ARM ELF, load dari microSD runtime)
- **Momentum** menambah ratusan lagi + community FAP store
- **Palanu** punya QuickJS app runtime + `.kapp` format, tapi: belum ada microSD, install volatile (hilang reboot), belum ada community, belum ada app store

#### MicroSD sebagai First-Class Storage (Palanu: ❌)
- Flipper: simpan apps, signals, RFID/NFC databases, animations, audio, IR codes
- Palanu: Plan 38 belum diimplementasi; MemFS volatile only

#### Battery + Charging UX (Palanu: ❌ dev-board; 🚧 SkyRizz E32 butuh)
- Flipper: LP battery 2000mAh, USB-C charge, % indicator, hibernation
- Palanu dev-board: tidak ada battery ADC. SkyRizz E32: hardware ada tapi driver belum.

#### Community & Documentation (Palanu: ❌)
- Flipper: >100k Discord, docs.flipper.net, qFlipper app, GitHub 10k stars
- Palanu: tidak ada public documentation, tidak ada community

#### Bootloader / DFU (Palanu: 🚧 Parsial)
- Flipper: recovery mode, DFU via USB, qFlipper auto-detect
- Palanu: OTA via KLP (ada), tapi tanpa signing/rollback; tidak ada DFU fallback UI

#### Momentum-specific (semua tidak ada di Palanu):
- Animated GIF home screen
- App theming / custom skins
- Extended sub-GHz protocols (dozens extra)
- WiFi scanner via companion ESP32-Marauder
- BLE keyboard/mouse emulation
- App reorder + hide in menu
- Custom boot animation
- GPIO app launcher
- UART/I2C/SPI terminal

### 2.2 Apa yang Palanu Miliki yang Flipper Tidak Punya

| Keunggulan Palanu | Keterangan |
|-------------------|------------|
| **WASM browser simulator** | Develop app tanpa hardware. Flipper hanya ada qFlipper (config), tidak ada firmware emulation |
| **JavaScript app SDK** | Tulis app dengan JS + JSX. Flipper memerlukan C code + ARM toolchain, compile ke `.fap` |
| **Modern flex layout UI** | Resolution-independent, declarative `ComponentApp`/UiNode tree. Flipper draw primitive (x,y) hardcoded |
| **KLP remote protocol** | Screen streaming, file manager, app install, CLI — semua via USB/BLE/web. Flipper tidak ada ini |
| **Forge web UI** | Simulator + remote + firmware flash dari browser. Flipper cuma qFlipper desktop app |
| **Touchscreen support** | SkyRizz E32 punya FT6336U capacitive touch. Flipper: tidak ada touchscreen |
| **Camera** | SkyRizz E32 punya GC2145. Flipper: tidak ada |
| **Audio (mic + speaker)** | SkyRizz E32. Flipper punya piezo buzzer saja |
| **Layered HAL** | core/platform/board/target — bersih, testable, hardware-agnostic. Flipper: #ifdef di mana-mana |
| **App crash isolation (JS)** | QuickJS isolasi JS fault. Flipper native FAP: crash = reboot |
| **TFT color display (SkyRizz E32)** | 240×320 IPS. Flipper: 128×64 1-bit monochrome |

---

## 3. Hak5 — Berbeda Target, Tapi Ada Overlap

### WiFi Pineapple
- **Fokus**: offensive WiFi security research (evil portal, deauth, WPA capture)
- **Runtime**: OpenWRT Linux + module system (bash scripts + web UI)
- **Overlap dengan Palanu**: WiFi scanning, web UI companion
- **Yang bisa diadopsi**: konsep "module/plugin for network tools" — kalau Palanu punya
  sub-GHz + WiFi tools yang bisa di-compose seperti Pineapple modules, itu kuat

### Bash Bunny / O.MG Cable / USB Rubber Ducky
- **Fokus**: USB injection attacks (DuckyScript execution, HID emulation)
- **Overlap**: BadUSB — Palanu punya TinyUSB CDC sekarang; **HID class bisa ditambah**
- **Yang bisa diadopsi**: DuckyScript player via USB HID — ini fitur yang bisa ada di
  Palanu dengan sedikit effort (TinyUSB HID + script runner app)

---

## 4. AkiraOS — Sudah Dikaji

Lihat `docs/research/akiraos-vs-kairo.md` untuk analisis lengkap.

**Ringkasan gap yang masih relevan (belum diadopsi):**
- Capability enforcement (advisory → enforced)
- App crash isolation + auto-restart
- Per-app memory quota
- Persistent app install (on-flash)
- Secure boot + OTA signing

---

## 5. M5Stack — Perbandingan UI & Ecosystem

| Aspek | M5Stack | Palanu |
|-------|---------|--------|
| Display | Color IPS 240×240 (M5Core2) atau 320×240 | 1-bit e-ink (dev-board) / TFT (SkyRizz E32) |
| UI Framework | LVGL (color, 80+ widgets) + UIFlow (Blockly/Python) | Custom flex (monochrome) + JS ComponentApp |
| App language | C++ (Arduino) + MicroPython | C++ (native) + JavaScript (QuickJS) |
| Module ecosystem | 50+ stackable modules (GPS, LoRa, RS485, fingerprint...) | Satu board, tidak ada modul stackable |
| Community | Large (banyak tutorial, M5Burner flash tool) | Belum ada community |
| Simulator | Tidak ada firmware simulator | WASM di browser ✅ |
| Remote protocol | Tidak ada | KLP (BLE/USB/web) ✅ |

**Yang bisa diadopsi dari M5Stack:**
- Konsep "module expansion" — GPIO + header untuk add-on (sub-GHz, NFC, LoRa)
- UIFlow adalah benchmark untuk low-barrier app development; JS kita lebih powerful tapi butuh dokumentasi setara
- M5Burner UX (satu-klik flash) mirip tujuan Forge

---

## 6. PineTime / InfiniTime — OTA & Polish Reference

PineTime bukan handheld hacking tool, tapi pelajaran OTA dan UX-nya relevan:

- **OTA via BLE companion app** (InfiniTime 1.12): reliable, rollback jika gagal boot
- **Watchface system**: user bisa load watchface baru via OTA tanpa reflash penuh
- **Firmware update UX** sangat polished: progress bar, checksum, auto-rollback
- **Battery life** dioptimasi agresif (hibernation, low-power BLE advertising)

**Yang bisa diadopsi:**
- Model OTA "partial update" (hanya update asset/app, bukan full firmware)
- UX progress bar untuk firmware flash di Forge
- Low-power mode yang proper

---

## 7. Meshtastic — LoRa Networking Reference

- **Yang bisa diadopsi**: arsitektur "channel" di Meshtastic mirip KLP kita, tapi untuk LoRa mesh. Kalau Palanu mau support LoRa di masa depan, Meshtastic jadi referensi bagus.
- **Tidak perlu ditiru sekarang** — beda target terlalu jauh.

---

## 8. PortaPack H2 (HackRF Addon) — SDR UI Reference

- C++ UI framework untuk SDR, embedded, monochrome display
- App "modules" yang bisa dipilih dari menu — mirip screen navigation Palanu
- **Yang bisa diadopsi**: konsep "signal capture apps" kalau kita mau support SDR-adjacent tools (sub-GHz analysis)

---

## 9. Feature Matrix — Palanu vs Field

| Fitur | Palanu | Flipper Zero | Momentum | M5Stack | AkiraOS |
|-------|--------|-------------|----------|---------|---------|
| Dynamic app install (runtime) | 🚧 Parsial (volatile) | ✅ FAP+microSD | ✅ | 🚧 SPIFFS | ✅ WASM OTA |
| Persistent app storage | ❌ | ✅ MicroSD | ✅ | ✅ SPIFFS/SD | ✅ LittleFS |
| JS/scripting for apps | ✅ QuickJS | ❌ C only | ❌ | ✅ MicroPython | ✅ WASM multi-lang |
| Browser simulator | ✅ WASM | ❌ | ❌ | ❌ | ❌ (native_sim only) |
| Remote screen view | ✅ KLP | ❌ | ❌ | ❌ | ❌ |
| Web companion | ✅ Forge | ✅ qFlipper | ✅ | ✅ M5Burner | ❌ |
| Color display | ✅ (SkyRizz E32) | ❌ | ❌ | ✅ | 🔄 |
| Touchscreen | ✅ (SkyRizz E32) | ❌ | ❌ | ✅ | ❌ |
| Camera | ✅ (SkyRizz E32) | ❌ | ❌ | ✅ | ❌ |
| Audio mic + speaker | ✅ (SkyRizz E32) | ❌ (buzzer only) | ❌ | ✅ | ❌ |
| Sub-GHz radio | ❌ | ✅ CC1101 | ✅ | 🔧 Module | 🔧 Module |
| NFC/RFID | ❌ | ✅ | ✅ | 🔧 Module | ❌ |
| Infrared | ❌ | ✅ | ✅ | 🔧 Module | ❌ |
| BadUSB/HID | ❌ | ✅ | ✅ | ❌ | ❌ |
| GPIO expansion | ❌ | ✅ | ✅ | ✅ | ❌ |
| BLE peripheral | ✅ | ✅ | ✅ | ✅ | ✅ |
| BLE scanning/central | ❌ | ✅ | ✅ | ✅ | ❌ |
| WiFi + HTTP | ✅ (unverified HW) | ✅ (via Marauder) | ✅ | ✅ | ✅ |
| Battery monitoring | ❌ | ✅ | ✅ | ✅ | ✅ |
| App capability enforcement | ❌ (advisory only) | N/A (no 3rd party runtime) | N/A | N/A | ✅ |
| App crash isolation | 🚧 JS only | 🚧 FAP watchdog | 🚧 | ❌ | ✅ WASM sandbox |
| Secure boot / OTA signing | ❌ | ✅ | ✅ | ❌ | ✅ MCUboot |
| Community app store | ❌ | ✅ 1000+ apps | ✅ extra | ✅ UIFlow store | ❌ |
| Community / docs | ❌ | ✅ Massive | ✅ | ✅ | 🚧 |
| Modern UI framework | ✅ Flex layout | ❌ Primitive draw | ❌ | 🔧 LVGL | ❌ 9 widget |
| Resolution independent | ✅ | ❌ | ❌ | 🔧 LVGL partial | ❌ |

---

## 10. Gap Analisis per Area

### 10.1 Area Kritis (showstopper sebelum MVP)

#### A. Persistent Storage — Plan 38
**Gap:** App install hilang setiap reboot. Ini satu-satunya hal yang membuat
product tidak bisa di-ship ke user nyata dalam kondisi apapun.

**Implementasi:**
- LittleFS internal flash (ESP-IDF sudah punya driver) untuk app bundle + config
- `/apps/<id>/` per-app directory di flash
- Load installed papps on boot sudah ada (`loadInstalledPapps()`) — tinggal mount
  nyata bukan MemFS

**Effort:** M | **Dampak:** Kritis

#### B. Hardware Stability — Current Bug
**Gap:** JS apps intermittent blank screen di SkyRizz E32. Hello app sudah
fixed. Counter/SysInfo kadang blank. Root cause: timing race antara `present()`
dan `draw()`, atau DPM/display flush issue.

**Effort:** S–M (investigation needed) | **Dampak:** Kritis

#### C. WiFi End-to-End on Hardware
**Gap:** WiFi + HTTP sudah build + sim ✅ tapi belum diverifikasi di hardware.
Tanpa ini, fitur Ticker/HTTP apps tidak bisa diklaim ready.

**Effort:** S (test + fix bugs) | **Dampak:** Tinggi

#### D. Real-Time Clock / NTP
**Gap:** Jam di status bar tidak tahu waktu nyata. Setiap boot mulai dari epoch.
NTP via WiFi belum diimplementasi.

**Implementasi:**
- `nema::ClockService` sudah ada; perlu `nema::NtpService` yang fetch NTP via UDP
  setelah WiFi connect, set `rt.clock().setEpochMs()`
- Event `NetworkConnected` → trigger NTP sync

**Effort:** S | **Dampak:** Tinggi (jam yang salah = alat tidak berguna)

### 10.2 Area Penting (MVP-minus tanpa ini)

#### E. USB HID / BadUSB
**Gap:** TinyUSB sudah ada (CDC-ACM). HID class perlu ditambah.

**Implementasi:**
- Tambah `TUSB_CLASS_HID` di `tusb_config.h` untuk composite CDC+HID
- `IUsbHid` interface + `Esp32UsbHid` driver
- `BadUsbApp`: parser DuckyScript minimal (STRING, DELAY, ENTER, CTRL+, etc.)
- Simpan scripts di `/badusb/` di microSD/flash

Ini adalah **killer feature paling mudah diimplementasi** — TinyUSB sudah ada,
DuckyScript parser ratusan baris, langsung relevan untuk security community.

**Effort:** M | **Dampak:** Tinggi (differentiator vs Flipper: JS scripting untuk BadUSB)

#### F. BLE Scanning / Central Mode
**Gap:** Saat ini hanya BLE peripheral (GATT server, advertising). Tidak bisa scan
device lain.

**Implementasi:**
- `IBleScanner` interface + `Esp32BleScanner` (NimBLE sudah support active scan)
- `BtScanApp`: tampilkan nearby BLE devices (MAC, name, RSSI, services)
- `BleSniffer`: capture advertising data

**Effort:** M | **Dampak:** Tinggi (BLE hacking tool populer di Flipper)

#### G. Battery Monitoring
**Gap:** Dev board tidak punya ADC untuk battery. SkyRizz E32 perlu implementasi.

**Implementasi (SkyRizz E32):**
- Cek apakah ada battery ADC di board schematic
- Atau: dummy driver yang estimasi dari waktu sejak boot (tidak akurat tapi UI lebih baik dari kosong)
- Status bar sudah punya slot untuk battery icon

**Effort:** S–M | **Dampak:** Sedang

#### H. App Crash Recovery
**Gap:** JS app yang crash (misalnya infinite loop atau runtime error) bisa hang
app thread. Tidak ada auto-restart.

**Implementasi:**
- `AppHostManager`: track restart count per app, backoff delay (1s → 5s → fail)
- `JsApp`: wrap `run()` dengan try/catch di level C++ + watchdog per AppHost
- Status: `RUNNING → CRASHED → RESTARTING → RUNNING` atau `FAILED`
- Logs screen tampilkan crash reason

**Effort:** M | **Dampak:** Penting untuk production stability

#### I. App Capability Enforcement
**Gap:** `rt.capabilities().has("wifi")` cuma advisory. App bisa bypass.

**Implementasi:**
- Saat `JsApp::onStart()`, inject hanya method yang ada di manifest `capabilities[]`
- Kalau `capabilities` tidak include `"net.wifi"`, jangan install bridge `nema.http.*`
- Untuk native apps: wrap call ke sensitive API dengan capability check + log + reject

**Effort:** M | **Dampak:** Fondasi keamanan

### 10.3 Area Nice-to-Have (post-MVP)

#### J. Sub-GHz Radio
**Gap:** Tidak ada hardware. Butuh modul CC1101 + SPI wiring.

Ini adalah fitur yang paling define Flipper Zero. Tanpa ini, Palanu tidak akan
pernah dianggap "Flipper alternative". Tapi ini butuh hardware revision.

**Implementasi (setelah hardware ada):**
- `ISubGhzDriver` interface + CC1101 SPI driver
- `SubGhzApp`: frequency scan, signal capture (OOK/ASK/FSK), replay
- Signal file format: JSON header + raw timing data (kompatibel format Flipper `.sub`?)

**Effort:** L (butuh hardware) | **Dampak:** Strategis

#### K. NFC / RFID
**Gap:** Tidak ada hardware. Butuh modul PN532 + I2C/SPI.

**Implementasi:**
- `INfcDriver` + PN532 driver
- `NfcApp`: read/write ISO 14443-A (MIFARE, NTAG), ISO 15693 (iClass)
- `RfidApp`: 125kHz reader (EM4100, HID Prox)

**Effort:** L (hardware dependent) | **Dampak:** Strategis

#### L. Infrared TX/RX
SkyRizz E32 punya kamera (GC2145) tapi bukan IR. Butuh IR LED + TSOP receiver.

**Effort:** M (hardware + software) | **Dampak:** Sedang

#### M. GPIO Expansion / Hardware Tool Apps
**Gap:** Tidak ada ekspansi GPIO yang user-accessible.

Idea: **ekspansi via USB-C header** — Palanu expose UART/SPI/I2C via USB serial
(CDC multi-port), mirip PortaPack. Apps bisa talk ke external hardware via
virtual COM port.

**Effort:** M | **Dampak:** Sedang

#### N. LVGL / Color UI Support (SkyRizz E32)
**Gap:** SkyRizz E32 punya TFT 240×320 IPS, tapi Palanu hanya punya 1-bit renderer.

**Implementasi:**
- `LvglServer`: `IDisplayServer` yang pakai LVGL untuk render ke physical framebuffer RGB565
- Pertahankan Aether (1-bit) sebagai default; LVGL hanya ketika app opt-in
- JS SDK: extend komponen dengan warna via `style.color`

**Effort:** L | **Dampak:** Tinggi (visual differentiator)

#### O. App Store / Marketplace
**Gap:** Tidak ada cara user install app selain KLP push dari Forge.

**Implementasi:**
- Registry JSON hosted di server (atau embedded sebagai URL list)
- `AppStoreApp`: browse, download, install dari device langsung
- Forge `/apps` page: browse community apps

**Effort:** L (butuh backend + community) | **Dampak:** Tinggi jangka panjang

---

## 11. Positioning Strategy

### Yang Membuat Palanu Berbeda (defensible differentiation)

1. **"Write apps with JavaScript"** — barrier paling rendah untuk app development embedded
2. **"Develop in the browser, deploy to device"** — WASM sim + KLP push
3. **Touchscreen + camera** — SkyRizz E32 punya hardware yang Flipper tidak punya
4. **Modern UI yang scale** — flex layout, bisa pindah display baru tanpa redesign semua screen
5. **Forge ecosystem** — satu web app untuk simulate, flash, install, remote

### Yang Harus Dibangun untuk Compete

Palanu perlu setidaknya **2 dari 3** ini untuk punya place di market:
1. **Protocol tools** (sub-GHz, NFC, IR) — mahal tapi essential untuk "hacker toy"
2. **Community app ecosystem** (100+ apps, store) — butuh waktu dan developer outreach
3. **"AI-native" apps** — contoh: kamera + model AI di edge, voice command via mic,
   LLM tool pada device — ini yang belum ada di Flipper/M5Stack/AkiraOS sama sekali

Option (3) adalah yang paling realistis sebagai **true differentiator** mengingat
SkyRizz E32 sudah punya kamera + mic. Palanu sebagai "AI-capable hacker toy dengan
JS SDK" adalah positioning yang unik.

---

## 12. Prioritas Implementasi (rekomendasi)

### Tier 0 — Fix sekarang (blocker MVP)
1. JS apps intermittent blank screen (hardware bug, investigate)
2. Persistent app storage — Plan 38 LittleFS
3. NTP time sync setelah WiFi connect
4. WiFi end-to-end hardware verification

### Tier 1 — MVP features (bulan 1–2)
5. USB HID class + DuckyScript BadUSB app
6. BLE scanning + device list app
7. App crash recovery + auto-restart
8. Battery monitoring (SkyRizz E32)
9. Capability enforcement (manifest-based gating)
10. OTA signing + rollback (secure boot ESP-IDF v2)

### Tier 2 — Post-MVP differentiators (bulan 3–4)
11. LVGL color renderer (SkyRizz E32)
12. AI inference app (camera + TFLite Micro atau ONNX)
13. Voice command via mic + basic STT
14. Community JS SDK docs + app template
15. Forge app store (hosted registry + in-device browser)

### Tier 3 — Roadmap (setelah community tumbuh)
16. Sub-GHz hardware modul + driver
17. NFC/RFID hardware modul + driver
18. Multi-board support (lebih dari 2 boards)
19. WAMR sebagai runtime app kedua (untuk performa + multi-bahasa)
