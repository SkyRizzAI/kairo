# 18 — ESP32 E-ink Display Driver (GxEPD2)

> Implementasi `IDisplayDriver` untuk panel e-ink fisik via GxEPD2. Target: GDEY027T91 2.7" 264×176 1-bit. Mengganti SimDisplay dengan driver hardware nyata — Canvas/UI Runtime jalan tanpa perubahan.

- Status: ☐ Not started
- Milestone: M6 (ESP32 Dev Hardware)
- Depends on: 13 (IDisplayDriver/Canvas), 16 (ESP32 Platform), 17 (Dev Board pinout)
- Blocks: UI di hardware nyata

---

## Goal

- `EinkDisplay : IDisplayDriver` membungkus GxEPD2 (`GxEPD2_BW<GxEPD2_270_GDEY027T91, ...>`).
- Resolusi 264×176, rotation 1 (landscape) — **identik dengan SimDisplay** sehingga Canvas/Screen tidak berubah.
- Event-driven flush: `flush()` = full refresh (~1s), partial-window untuk cursor update.
- 1-bit buffer Kairo (0=bg/putih, 1=ink/hitam) → GxEPD2 drawPixel.

## Scope

### In scope

- `EinkDisplay` implement `IDisplayDriver`: drawPixel, fillRect, clear, flush, invertRect.
- GxEPD2 init (SPI pins dari `board_config.h`).
- Mapping: Kairo `on=true` (ink) → GxEPD2 `GxEPD_BLACK`; `on=false` → `GxEPD_WHITE`.
- Full refresh on `flush()`; opsional partial refresh untuk update kecil.
- E-ink prewarm setelah full refresh (seperti ref `screen_prewarm`).

### Out of scope

- Grayscale (panel ini 1-bit B/W).
- Partial-refresh optimization tahap awal (mulai full refresh dulu).
- Display driver IC lain.

---

## Design

### Dependency (sama dengan ref)

- `arduino-esp32` v3.3.8 (component registry).
- `GxEPD2` + `Adafruit_GFX` + `Adafruit_BusIO` — sebagai EXTRA_COMPONENT_DIRS (vendor di `firmware/vendor/arduino-libs/` atau pakai component manager).

> Catatan: ref menaruh lib ini di `software/components` di luar repo. Untuk Kairo, **vendor sources** di `firmware/vendor/arduino-libs/{GxEPD2,Adafruit_GFX,Adafruit_BusIO}` agar build self-contained.

### File

```text
firmware/platforms/esp32/include/kairo/esp32/eink_display.h
firmware/platforms/esp32/src/eink_display.cpp
```

### EinkDisplay

```cpp
#include <GxEPD2_BW.h>
#include <gdey/GxEPD2_270_GDEY027T91.h>

class EinkDisplay : public IDisplayDriver, public IService {
public:
    EinkDisplay();  // construct GxEPD2 with pins from board_config

    // IDriver
    const char* name() const override { return "EinkDisplay"; }
    DriverKind  kind() const override { return DriverKind::Display; }

    // IDisplayDriver
    uint16_t width()  const override { return 264; }
    uint16_t height() const override { return 176; }
    void drawPixel(uint16_t x, uint16_t y, bool on) override;
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) override;
    void clear(bool on = false) override;
    void flush() override;       // full-window refresh
    void invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) override;

    // IService
    void start() override;       // display.init(115200,...); setRotation(1)
    void stop()  override;
    void tick(uint64_t) override {}

private:
    using Panel = GxEPD2_BW<GxEPD2_270_GDEY027T91, GxEPD2_270_GDEY027T91::HEIGHT>;
    Panel    epd_;
    uint8_t  buf_[264 * 176];   // shadow buffer (for invertRect read-modify-write)
    Logger*  log_ = nullptr;
};
```

### flush() — full refresh

GxEPD2 pakai paged drawing. Karena Kairo Canvas menggambar ke shadow `buf_` dulu, `flush()` tinggal mem-blit buffer:

```cpp
void EinkDisplay::flush() {
    epd_.setFullWindow();
    epd_.firstPage();
    do {
        for (uint16_t y = 0; y < 176; y++)
            for (uint16_t x = 0; x < 264; x++)
                epd_.drawPixel(x, y, buf_[y*264+x] ? GxEPD_BLACK : GxEPD_WHITE);
    } while (epd_.nextPage());
    // prewarm charge pump untuk partial refresh berikutnya (opsional)
}
```

### drawPixel/fillRect/invertRect → shadow buffer

Semua drawing menulis ke `buf_` (sama seperti SimDisplay), `flush()` baru push ke panel. Ini membuat:
- `invertRect` bisa read-modify-write dari `buf_`.
- Mengurangi e-ink writes (1 refresh per flush, bukan per pixel).

### init

```cpp
void EinkDisplay::start() {
    SPI.begin(devboard::PIN_EPD_SCK, -1, devboard::PIN_EPD_MOSI, devboard::PIN_EPD_CS);
    epd_.init(115200, true, 10, false);
    epd_.setRotation(1);   // landscape 264×176
    clear(false);
    flush();
    log_->info("EinkDisplay", "started", {{"panel","GDEY027T91"},{"res","264x176"}});
}
```

Konstruktor `epd_` butuh pin CS/DC/RST/BUSY:
```cpp
EinkDisplay::EinkDisplay()
    : epd_(GxEPD2_270_GDEY027T91(devboard::PIN_EPD_CS, devboard::PIN_EPD_DC,
                                  devboard::PIN_EPD_RST, devboard::PIN_EPD_BUSY)) {}
```

### Esp32Platform integration

```cpp
void Esp32Platform::registerDrivers(Runtime& rt) {
    // ... wifi, battery
    display_.init(rt.log());
    rt.container().registerService(&display_);
    rt.container().registerAs<IDisplayDriver>(&display_);
}
```

Runtime membuat `Canvas` dari `IDisplayDriver` (sudah ada di plan 13/14) — **tanpa tahu** ini e-ink atau simulator.

---

## Tasks

- [ ] Vendor `GxEPD2`, `Adafruit_GFX`, `Adafruit_BusIO` ke `firmware/vendor/arduino-libs/`.
- [ ] `EinkDisplay` (shadow buffer + GxEPD2 blit on flush).
- [ ] init SPI + GxEPD2 (pins dari board_config).
- [ ] drawPixel/fillRect/clear/invertRect → shadow buffer.
- [ ] `flush()` full-window refresh.
- [ ] Register di Esp32Platform.
- [ ] EXTRA_COMPONENT_DIRS di target CMakeLists.
- [ ] Verifikasi: HomeScreen muncul di e-ink fisik, navigasi tombol refresh layar.

## Acceptance criteria

- `clear(false); flush()` → panel putih bersih.
- HomeScreen render di e-ink (logo KAIRO + status bar + app list) — visual sama dengan simulator.
- Tekan UP/DOWN → cursor pindah, e-ink refresh.
- Frame buffer identik dengan SimDisplay (bisa cross-check: dump buf_ vs frame JSON simulator).

## Risks / notes

- **Full refresh ~1s** — wajar untuk e-ink. UX harus event-driven (sudah, lewat ViewDispatcher). Partial refresh untuk cursor bisa ditambah nanti (`setPartialWindow`).
- **Ghosting** pada partial refresh perlu tuning per panel — mulai full refresh dulu.
- GxEPD2 `drawPixel` per-pixel di paged loop agak lambat; kalau perlu, pakai `drawImage`/`writeImage` dengan packed 1-bit buffer untuk transfer lebih cepat.
- Shadow buffer 264×176 = 46KB — taruh di PSRAM (`heap_caps_malloc(MALLOC_CAP_SPIRAM)`) atau static (cukup di DRAM ESP32-S3).
- Cross-verify keuntungan arsitektur: karena SimDisplay & EinkDisplay sama-sama `IDisplayDriver` 1-bit 264×176, **UI yang sudah jalan di simulator dijamin render identik di e-ink**.
