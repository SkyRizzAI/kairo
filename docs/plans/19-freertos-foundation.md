# 19 — Async Display & Event Foundation

> **Cakupan jujur:** plan ini HANYA menyelesaikan 2 sumber blocking/race yang nyata ada saat itu — (1) freeze e-ink dipindah ke task terpisah, (2) race event WiFi lintas-core diamankan via queue. Ini **BUKAN** kernel non-blocking lengkap. Model multi-thread lengkap ada di **plan 19.5**, bukan di sini. Judul lama "Non-Blocking Architecture" over-promise dan sudah dikoreksi.

- Status: ✅ Implemented & verified build (host + ESP32)
- Milestone: M6.5 (Async Foundation)
- Depends on: 16 (ESP32 Platform), 17 (Dev Board), 14 (UI Runtime)
- Blocks: 19.5 (Kernel) — fondasi parsial yang dipakai ulang di sana

---

## Latar Belakang & Masalah

ESP32-S3 adalah **dual-core**. FreeRTOS selalu running di bawah. Tapi arsitektur Kairo saat ini:

```
Core 0 (loopTask):     rt.step() → EventBus::publish() → iterasi subs_[]
Core 1 (sys_evt task): WiFi event → events_->publish()  → iterasi subs_[] BERSAMAAN
```

Ini **data race nyata** — `std::vector` tanpa proteksi bisa corrupt. Bukan "mungkin terjadi", tapi **pasti terjadi** di dual-core jika WiFi aktif.

Masalah kedua: e-ink refresh 1-2 detik **blocking** di main task — selama itu tidak ada input, plugin, atau event yang diproses.

### Yang sudah diimplementasikan (tapi belum generik)

| Komponen | Status | Catatan |
|---|---|---|
| Logger `std::mutex` | ✅ Done | Generik, tidak perlu diubah |
| `Esp32WifiDriver` cross-task queue | ✅ Done | **Tidak generik** — pattern harus diextract |
| `EinkDisplay` FreeRTOS display task | ✅ Done | **Tidak generik** — hard-coded di driver |

### Masalah dengan implementasi sekarang

**DRY violation:** Kalau besok ada `BleDriver`, `NtpService`, atau `HttpClient` yang perlu publish event dari background task — mereka harus copy-paste pattern yang sama (`std::queue<Event> + std::mutex + drain di tick()`). Kalau ada `OledDisplay` atau `SpiDisplay` lain — harus copy-paste double-buffer + FreeRTOS task.

---

## Arsitektur yang Benar

Pisahkan **mekanisme** dari **implementasi driver**. Driver cukup implementasikan operasi synchronous-nya. Framework menyediakan wrapper untuk non-blocking.

```
┌─────────────────────────────────────────────────────────┐
│ CORE FRAMEWORK                                          │
│                                                         │
│  AsyncEventPoster       AsyncDisplayDriver              │
│  ─────────────────       ──────────────────             │
│  post(Event)            wraps IDisplayDriver            │
│  └→ queue + mutex        double-buffer + FreeRTOS task  │
│  flush(EventBus&)        flush() non-blocking           │
│                                                         │
└─────────────────────────────────────────────────────────┘
        ↑ dipakai oleh              ↑ dipakai oleh
        
┌──────────────┐              ┌────────────────┐
│ Esp32WifiDriver│              │ EinkDisplay     │
│ (hanya STA    │              │ (hanya SPI +    │
│  connect/event│              │  GxEPD2, sync)  │
└──────────────┘              └────────────────┘
```

Driver **tidak perlu tahu** tentang threading. Framework yang urus.

---

## Komponen yang Perlu Dibuat/Refactor

### 1. `AsyncEventPoster` (core, baru)

**File:** `firmware/core/include/kairo/event/async_event_poster.h`

```cpp
// Thread-safe event poster untuk background task (WiFi, BLE, HTTP, dll).
// Background task memanggil post() dari task manapun.
// Main task memanggil flush() setiap frame di rt.step().
class AsyncEventPoster {
public:
    void post(Event e);          // thread-safe — dari task manapun
    void flush(EventBus& bus);   // drain ke EventBus — main task only

private:
    std::queue<Event> queue_;
    std::mutex        mutex_;
};
```

**Cara pakai di driver:**
```cpp
// Di Esp32WifiDriver (atau BleDriver, NtpService, dll):
// Ganti: events_->publish(e)
// Dengan: poster_->post(e)

// Di Runtime::step():
asyncPoster_.flush(events());   // drain semua pending events
```

**Keuntungan:** Semua driver background cukup `poster_->post(e)`. Tidak ada lagi `std::queue + std::mutex` per-driver.

### 2. `AsyncDisplayDriver` (core, baru)

**File:** `firmware/core/include/kairo/hal/async_display.h`

```cpp
// Wrapper yang membuat IDisplayDriver apapun menjadi non-blocking.
// Double buffer + dedicated FreeRTOS task untuk flush.
// Driver (EinkDisplay, OledDisplay, dll) tetap implement IDisplayDriver
// secara synchronous — AsyncDisplayDriver yang urus threading.
class AsyncDisplayDriver : public IDisplayDriver {
public:
    // Wrap driver yang ada + start FreeRTOS task
    void init(IDisplayDriver& driver, Logger& log);

    // IDisplayDriver — semua operasi gambar ke draw_buf_ (non-blocking)
    uint16_t width()  const override;
    uint16_t height() const override;
    void drawPixel(uint16_t x, uint16_t y, bool on) override;
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) override;
    void clear(bool on = false) override;
    void invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) override;
    void flush() override;  // non-blocking: swap + signal task

    void stop();

private:
    static constexpr size_t MAX_PIXELS = 264 * 176; // max e-ink size
    
    IDisplayDriver* inner_      = nullptr;
    uint8_t*        draw_buf_   = nullptr;  // main task writes
    uint8_t*        flush_buf_  = nullptr;  // display task reads
    uint8_t*        prev_buf_   = nullptr;  // last sent (dirty-rect)
    uint8_t         partial_count_ = 0;
    size_t          buf_size_   = 0;

    SemaphoreHandle_t flip_sem_   = nullptr; // idle=given, busy=taken
    SemaphoreHandle_t signal_sem_ = nullptr;
    TaskHandle_t      task_       = nullptr;

    static void taskFn(void* arg);
    void        doFlush();
};
```

**Cara pakai:**
```cpp
// Di DevBoard::start() (atau platform setup):
einkDisplay_.init(log);   // EinkDisplay hanya setup SPI + GxEPD2
asyncDisplay_.init(einkDisplay_, log);  // AsyncDisplayDriver wraps it

// Runtime pakai asyncDisplay_ untuk Canvas
// EinkDisplay tidak perlu tahu tentang double buffer atau FreeRTOS
```

**Keuntungan:**
- `EinkDisplay` kembali menjadi driver sederhana (hanya SPI + GxEPD2)
- Semua display future (`OledDisplay`, `LcdDriver`) otomatis bisa non-blocking
- Simulator: tidak pakai `AsyncDisplayDriver` (SimDisplay sudah fast, tidak perlu)

### 3. Refactor `EinkDisplay` (boards/dev-board)

Setelah `AsyncDisplayDriver` ada, `EinkDisplay` dikembalikan ke versi sederhana:
- Hapus `buf_a_`, `buf_b_`, `draw_buf_`, `flush_buf_`, pointer swap
- Hapus FreeRTOS task + semaphore dari `EinkDisplay`
- `EinkDisplay::flush()` kembali synchronous (blocking — tapi tidak masalah karena `AsyncDisplayDriver` yang wrap)
- Hanya berisi: SPI init, GxEPD2 paged draw, dirty rect logic

### 4. Refactor `Esp32WifiDriver`

Setelah `AsyncEventPoster` ada:
- Hapus `pendingEvents_` + `pendingMutex_` dari driver
- Hapus `enqueue()` + swap trick dari `tick()`
- Driver menyimpan `AsyncEventPoster* poster_`
- `onWifiEvent()` → `poster_->post(e)` — lebih bersih
- `tick()` tidak perlu lagi (atau tetap kosong)

### 5. `Runtime` — integrasi kedua komponen

**`Runtime`** menjadi pemilik `AsyncEventPoster` dan mengintegrasikan drain ke `step()`:

```cpp
// Di runtime.h (private):
AsyncEventPoster asyncPoster_;

// Di runtime.cpp step():
asyncPoster_.flush(events());    // drain cross-task events ke EventBus
serviceManager_->tickAll(now);
// ...
```

Driver yang perlu post dari background task mendapat referensi ke `asyncPoster_` saat init:
```cpp
// Di Esp32Platform::registerDrivers():
wifi_.init(rt.log(), rt.asyncPoster());
```

---

## Scope

### In scope (plan ini)

- [ ] `AsyncEventPoster` — implement + test di simulator (std::mutex)
- [ ] `AsyncDisplayDriver` — implement (ESP32 only: FreeRTOS; simulator: skip/passthrough)
- [ ] Refactor `EinkDisplay` — kembalikan ke synchronous sederhana
- [ ] Refactor `Esp32WifiDriver` — gunakan `AsyncEventPoster`
- [ ] `Runtime` — integrasi `asyncPoster_` + drain di `step()`
- [ ] Update `DevBoard::start()` — setup `AsyncDisplayDriver` wrapping `EinkDisplay`
- [ ] Update `Esp32Platform::registerDrivers()` — pass `asyncPoster_` ke driver
- [ ] Dokumentasi pattern untuk driver masa depan

### Out of scope

- Per-plugin task isolation (plugins tetap di main task)
- FreeRTOS task priority tuning
- Stack size profiling
- Watchdog integration
- Simulator multi-thread (tidak dibutuhkan — simulator single-thread)

---

## File Plan

| File | Action | Keterangan |
|---|---|---|
| `core/include/kairo/event/async_event_poster.h` | **Buat baru** | Thread-safe event queue |
| `core/src/event/async_event_poster.cpp` | **Buat baru** | |
| `core/include/kairo/hal/async_display.h` | **Buat baru** | Non-blocking display wrapper |
| `core/src/hal/async_display.cpp` | **Buat baru** | ESP32: FreeRTOS; else: passthrough |
| `core/include/kairo/runtime.h` | Modify | Tambah `asyncPoster_`, `asyncPoster()` method |
| `core/src/runtime.cpp` | Modify | Drain asyncPoster_ di step() |
| `core/CMakeLists.txt` | Modify | Tambah source baru |
| `platforms/esp32/include/.../esp32_wifi_driver.h` | Refactor | Hapus queue lama, pakai `AsyncEventPoster*` |
| `platforms/esp32/src/esp32_wifi_driver.cpp` | Refactor | |
| `platforms/esp32/include/.../esp32_platform.h` | Modify | Tambah `AsyncDisplayDriver`? atau di board layer |
| `boards/dev-board/include/.../eink_display.h` | Refactor | Kembalikan ke sederhana (no FreeRTOS) |
| `boards/dev-board/src/eink_display.cpp` | Refactor | |
| `boards/dev-board/src/dev_board.cpp` | Modify | Init `AsyncDisplayDriver` wrapping `EinkDisplay` |

---

## Urutan Implementasi

```
1. AsyncEventPoster (core, baru)
   → Test: build simulator, WiFi inject masih jalan
   
2. Refactor Esp32WifiDriver (pakai AsyncEventPoster)
   → Test: build esp32, WiFi events masih publish

3. Runtime — integrasi asyncPoster_ + drain di step()
   → Test: simulator, flash dev board

4. AsyncDisplayDriver (core, baru)
   → Conditional compile: ESP32=FreeRTOS task, host=passthrough sync
   
5. Refactor EinkDisplay (kembalikan ke sederhana)
   → Test: flash dev board, display tetap jalan non-blocking

6. DevBoard setup AsyncDisplayDriver wrapping EinkDisplay
   → Test: flash, e-ink update tidak block input
```

---

## Conditional Compilation Strategy

`AsyncDisplayDriver` perlu FreeRTOS di ESP32 tapi tidak di simulator. Dua opsi:

**Opsi A — `#ifdef ESP_PLATFORM`** (sederhana, tapi messy):
```cpp
#ifdef ESP_PLATFORM
    xTaskCreate(...);
#else
    // synchronous passthrough
#endif
```

**Opsi B — Platform provides task abstraction** (bersih, recommended):
```cpp
// core/include/kairo/os/task.h
#ifdef ESP_PLATFORM
    // wraps xTaskCreate
#else
    // wraps std::thread (atau no-op jika simulator tidak butuh)
#endif
```

Untuk saat ini pakai **Opsi A** — simple, sudah proven. Refactor ke Opsi B nanti saat ada kebutuhan task lain (BLE, HTTP).

Letakkan `#ifdef ESP_PLATFORM` hanya di file `async_display.cpp` (implementasi), bukan di header. Header tetap bersih — consumer tidak perlu tahu.

---

## Acceptance Criteria

- [ ] `AsyncEventPoster` bisa dipakai dari task manapun tanpa crash/corruption
- [ ] WiFi events masih publish ke EventBus setelah refactor `Esp32WifiDriver`
- [ ] `EinkDisplay` tidak lagi mengandung FreeRTOS primitif
- [ ] `AsyncDisplayDriver` membuat e-ink non-blocking: tekan tombol saat refresh tidak freeze
- [ ] Simulator tetap build dan jalan (single-thread, tanpa display task)
- [ ] Semua test sebelumnya (partial refresh, dirty rect, partial count) tetap berjalan
- [ ] Driver baru (BLE, HTTP, OTA) tinggal pakai `AsyncEventPoster` tanpa setup apapun

---

## Catatan Pattern untuk Driver Masa Depan

Setelah plan ini selesai, **semua driver** background mengikuti pattern ini:

```cpp
// Driver yang perlu post event dari background task:
class FutureDriver : public ISomeDriver, public IService {
    AsyncEventPoster* poster_ = nullptr;

    void init(AsyncEventPoster& p) { poster_ = &p; }

    // Dari background task:
    void onBackgroundEvent() {
        poster_->post({events::SomethingHappened, {{"key", "value"}}});
        // Tidak ada mutex, tidak ada queue setup — poster mengurusnya
    }
};
```

**Driver yang perlu display (bukan layar utama, misal status LED):**
```cpp
// Cukup implement IDisplayDriver synchronous.
// AsyncDisplayDriver yang otomatis membuatnya non-blocking.
// Driver tidak perlu tahu FreeRTOS.
```

Ini adalah **kontrak** yang harus diikuti semua driver di Kairo — tidak ada FreeRTOS primitif langsung di driver, semua lewat abstraksi yang disediakan framework.
