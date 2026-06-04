# 16 — ESP32 Platform

> Implementasi `IPlatform` untuk ESP32 menggunakan ESP-IDF. Mengganti semua stub/dummy driver dengan driver hardware nyata. Target: **Kairo Dev Board** = ESP32-S3-WROOM-1 + e-ink (tier 2, lihat overview §0 — device testing sementara). Platform `esp32` ini nanti dipakai ulang oleh Kairo Board V1 tanpa berubah.

- Status: ☐ Not started
- Milestone: M6 (ESP32 Dev Hardware)
- Depends on: 06 (HAL interface), 08 (registries), plan 01–11 (core stable)
- Blocks: 17 (Kairo Dev Board), 18 (e-ink display driver)

---

## Goal

- `Esp32Platform : IPlatform` yang compile dengan ESP-IDF toolchain.
- `Esp32Clock`: implementasi `IClock` via `esp_timer_get_time()` + `gettimeofday`.
- `Esp32WifiDriver`: implementasi `IWifiDriver` via `esp_wifi` API.
- Build system: CMake + ESP-IDF component model (`idf_component_register`).
- Binary di-flash ke ESP32-S3 dan boot runtime penuh.

> **Catatan implementasi:** `Esp32BatteryDriver` **tidak** dibuat untuk Kairo Dev Board — `badge_pins.h` (ref) tidak punya pin battery ADC, jadi hardware-nya memang tidak ada. Capability `"battery"` tidak didaftarkan di dev board (capability-driven: UI cukup tidak menampilkan baterai). Battery monitoring menyusul di Kairo Board V1 yang PCB-nya didesain dengan pembagi tegangan baterai.

## Scope

### In scope

- `Esp32Platform` (registerDrivers, idle via `vTaskDelay`).
- `Esp32Clock` via `esp_timer_get_time()` (monotonic) + `gettimeofday` (epochMs).
- `Esp32WifiDriver`: connect/disconnect, event callback via ESP-IDF event loop.
- `Esp32BatteryDriver`: baca ADC channel, konversi ke persen (kalibrasi sederhana).
- `Esp32Logger` output: ke UART via `ESP_LOGI`/`printf` (sesuai ESP-IDF logging).
- `ConsoleSink` tetap dipakai — ESP-IDF stdout ke UART secara default.
- Build scripts: `firmware/tools/build-esp32.sh`, `firmware/tools/flash-esp32.sh`.
- CMakeLists ESP-IDF-style di `firmware/targets/dev-board/`.

### Out of scope

- Display driver ESP32 (plan 18 — SPI ke panel).
- Bluetooth, NFC, RFID, IR, SubGHz (Milestone 7+).
- OTA update.
- Plugin loading dari SD card.

---

## Design

### Repo structure

```text
firmware/
├─ platforms/
│  └─ esp32/
│     ├─ include/kairo/esp32/
│     │  ├─ esp32_platform.h
│     │  ├─ esp32_clock.h
│     │  ├─ esp32_wifi_driver.h
│     │  └─ esp32_battery_driver.h
│     ├─ src/
│     │  ├─ esp32_platform.cpp
│     │  ├─ esp32_clock.cpp
│     │  ├─ esp32_wifi_driver.cpp
│     │  └─ esp32_battery_driver.cpp
│     └─ CMakeLists.txt          # idf_component_register(...)
├─ boards/
│  └─ dev-board/                 # plan 17
├─ targets/
│  └─ dev-board/
│     ├─ main/
│     │  ├─ main.cpp             # mirip simulator main, pakai Esp32Platform
│     │  └─ CMakeLists.txt
│     ├─ CMakeLists.txt          # top-level ESP-IDF project
│     └─ sdkconfig.defaults      # menuconfig defaults
└─ tools/
   ├─ build-esp32.sh
   └─ flash-esp32.sh
```

### Esp32Clock

```cpp
class Esp32Clock : public IClock {
    uint64_t millis() override {
        return (uint64_t)(esp_timer_get_time() / 1000ULL);
    }
    uint64_t epochMs() override {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }
};
```

### Esp32WifiDriver

```cpp
class Esp32WifiDriver : public IWifiDriver, public IService {
public:
    void start() override;    // esp_wifi_init, register event handler
    void stop()  override;    // esp_wifi_disconnect + deinit
    bool connect(const char* ssid) override;  // esp_wifi_set_config + esp_wifi_connect
    void disconnect() override;
    bool isConnected() const override { return connected_; }
    // ...
private:
    static void wifiEventHandler(void* arg, esp_event_base_t base,
                                  int32_t id, void* data);
    volatile bool connected_ = false;
    char ssid_[33] = {};
};
```

WiFi event handler memanggil `events_->publish(NetworkConnected/Disconnected)` — sama dengan SimWifiDriver.

### Esp32BatteryDriver

```cpp
class Esp32BatteryDriver : public IBatteryDriver, public IService {
    // Baca ADC tiap tick (throttled ke 1 detik)
    // Konversi voltage → persen via linear map (kalibrasi per hardware)
    // Publish BatteryChanged saat level berubah
};
```

Konfigurasi ADC channel/unit via board header (plan 17).

### Esp32Platform

```cpp
class Esp32Platform : public IPlatform {
    const char* name() const override { return "esp32"; }
    IPlatform::OutputMode outputMode() const override {
        return OutputMode::Human;  // ESP32 selalu human mode (UART)
    }
    void idle() override {
        vTaskDelay(pdMS_TO_TICKS(5));  // yield ke FreeRTOS
    }
    void registerDrivers(Runtime& rt) override;
private:
    Esp32Clock         clock_;
    Esp32WifiDriver    wifi_;
    Esp32BatteryDriver battery_;
};
```

### Build system

ESP-IDF menggunakan CMake component model. `targets/dev-board/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platforms/esp32
    ${CMAKE_CURRENT_SOURCE_DIR}/../../boards/dev-board
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(kairo-dev-board)
```

Core component `idf_component_register`:

```cmake
# firmware/core/CMakeLists.txt (ESP-IDF variant)
idf_component_register(
    SRCS "src/runtime.cpp" "src/log/..." ...
    INCLUDE_DIRS "include"
)
```

**Penting**: Core CMakeLists.txt harus support **dua mode build**:
- Host (CMake biasa, untuk simulator) — sudah ada
- ESP-IDF (idf_component_register) — ditambah dengan CMake condition:

```cmake
if(ESP_PLATFORM)
    idf_component_register(SRCS ... INCLUDE_DIRS ...)
else()
    add_library(kairo_core STATIC ...)
    target_include_directories(kairo_core PUBLIC include)
endif()
```

### `firmware/tools/build-esp32.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../targets/dev-board"
idf.py build
```

### `firmware/tools/flash-esp32.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail
PORT=${1:-/dev/cu.usbmodem*}
cd "$(dirname "$0")/../targets/dev-board"
idf.py -p "$PORT" flash monitor
```

### Root package.json scripts

```json
"build:esp32": "bash firmware/tools/build-esp32.sh",
"flash:esp32": "bash firmware/tools/flash-esp32.sh"
```

---

## Tasks

- [ ] `Esp32Clock` (esp_timer + gettimeofday).
- [ ] `Esp32WifiDriver` (esp_wifi API + event handler + publish events).
- [ ] `Esp32BatteryDriver` (ADC + level calc + throttled tick).
- [ ] `Esp32Platform` (registerDrivers + idle via vTaskDelay).
- [ ] Dual-mode `core/CMakeLists.txt` (host vs ESP-IDF).
- [ ] `firmware/platforms/esp32/CMakeLists.txt` (idf_component_register).
- [ ] `firmware/targets/dev-board/CMakeLists.txt` + `main/main.cpp`.
- [ ] `sdkconfig.defaults` (PSRAM octal, exceptions+RTTI, Arduino-as-component, stack size).
- [ ] `partitions.csv` (factory app 3 MB — default ~1 MB terlalu kecil untuk arduino-esp32; binary ~1.07 MB).
- [ ] `build-esp32.sh` + `flash-esp32.sh`.
- [ ] Root package.json: `build:esp32`, `flash:esp32`.
- [ ] Verifikasi: flash ke Dev Board, lihat log boot via serial monitor.

## Acceptance criteria

- `idf.py build` sukses tanpa error.
- Flash ke ESP32-S3, serial monitor menampilkan boot sequence: Logger Initialized → Core ready → services Running.
- `Esp32WifiDriver.connect("ssid")` → log `NetworkConnected`.
- Core source (`firmware/core/**`) tidak berubah sama sekali — hanya platform/board yang baru (terverifikasi: host sim tetap build).

## Prasyarat development environment

```bash
# macOS
brew install esp-idf  # atau ikuti panduan resmi Espressif
. $IDF_PATH/export.sh
```

ESP-IDF versi target: **v5.x** (stable terbaru).

## Risks / notes

- Dual-mode CMakeLists perlu diuji dua-duanya setiap kali ada perubahan core — tambahkan ke CI nanti.
- `std::string`/`std::vector` di core: ESP-IDF + ESP32-S3 punya cukup heap (8MB PSRAM). Enable PSRAM di `sdkconfig.defaults`. Kalau nanti butuh di board tanpa PSRAM, perlu audit alokasi.
- FreeRTOS task: runtime berjalan di `app_main` task. Jika nanti perlu multicore, buat task terpisah untuk UI render dan plugin.
- WiFi credential: jangan hardcode di source. Gunakan `sdkconfig` atau NVS storage. Untuk development, boleh via `idf.py menuconfig`.
