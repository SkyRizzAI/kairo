# 28 — SkyRizz E32 Board Support

> Board layer baru untuk hardware produksi pertama: **SkyRizz E32**
> (`ESP32-S3-WROOM-1-N16R8` + TFT LCD + XL9535 I/O expander + 3 tombol fisik).
> Memanfaatkan Plan 27 (Input Abstraction) untuk keymap 3-tombol dan Plan 25
> (Adaptive UI) untuk resolusi LCD yang berbeda dari dev board.

- Status: ☐ Not started
- Milestone: M8 (Hardware Portability)
- Depends on: 16 (ESP32 Platform), 17 (Dev Board — referensi pola), 25 (Adaptive UI), 27 (Input Abstraction), 24 (Config Store)
- Blocks: —

---

## Hardware overview

Single source of truth: `dev-board-1-pin_map.md` + `dev-board-1-pin_cap.md`.

### Komponen utama

| Komponen | Part | Firmware path |
|---|---|---|
| MCU | ESP32-S3-WROOM-1-N16R8 | — |
| Display | TFT LCD via FPC1 | SPI: GPIO12/13/14/21; backlight via XL9535 P00 |
| I/O Expander | XL9535 (16-bit) | I2C addr `0x20`, INT di GPIO43 |
| Tombol SW1 | Local switch 1 | XL9535 P12, net `P8` |
| Tombol SW2 | Local switch 2 | XL9535 P04, net `P9` |
| Tombol PB1 | Push button 1 | XL9535 P05, net `P10` |
| Tombol PB2 | Push button 2 | XL9535 P06, net `P11` |
| Tombol SW3 | Local switch 3 | XL9535 P11, net `P7` (shared ext. IO3) |
| Touch | TSC2007 (resistif) | I2C + PENIRQ GPIO2; backlog Plan 29 |
| Sensor suhu/kelembapan | AHT20 | I2C GPIO47/48 |
| Sensor cahaya | LTR-303ALS | I2C GPIO47/48 |
| Akselerometer | SC7A20 | I2C GPIO47/48 |
| RGB LED | WS2812 ×2 | GPIO46 serial chain |
| Indicator LED | IND | XL9535 P17 |
| NVS (Config Store) | Flash | Sudah dikonfigurasi di partition table |
| Secure element | SE050 | I2C + XL9535 P03 reset (Plan terpisah) |
| microSD | TF1 | SPI GPIO40/41/42/44 (Plan terpisah) |

### Pin summary kritis

| Signal | GPIO | Notes |
|---|---|---|
| I2C SDA | GPIO47 | Shared: XL9535, AHT20, LTR-303ALS, SC7A20, TSC2007, SE050 |
| I2C SCL | GPIO48 | Shared |
| XL9535 INT# | GPIO43 | Active-LOW; interrupt dari semua expander event |
| LCD SCLK | GPIO12 | SPI direct |
| LCD DC | GPIO13 | |
| LCD CS | GPIO14 | Active-LOW |
| LCD MOSI | GPIO21 | |
| LCD Backlight | XL9535 P00 | LOW = off, HIGH = on (via transistor Q4) |
| Touch PENIRQ | GPIO2 | Active-LOW IRQ dari TSC2007 |
| RGB LED | GPIO46 | WS2812 data |

---

## Desain board config

### `board_config.h`

```cpp
#pragma once
namespace kairo::skyrizze32 {

// I²C bus (shared: XL9535, sensors, touch, SE050)
constexpr int PIN_SCL     = 48;
constexpr int PIN_SDA     = 47;
constexpr int PIN_BUS_INT = 43;  // XL9535 INT# (active-LOW)

// LCD SPI (direct ESP32)
constexpr int PIN_LCD_SCLK = 12;
constexpr int PIN_LCD_DC   = 13;
constexpr int PIN_LCD_CS   = 14;
constexpr int PIN_LCD_MOSI = 21;
// MISO: not connected (write-only LCD)
// Backlight: via XL9535 P00 (not direct GPIO)

// Touch interrupt (TSC2007)
constexpr int PIN_TS_INT  = 2;   // PENIRQ, active-LOW

// RGB LED chain
constexpr int PIN_RGB     = 46;

// XL9535 I²C address (A0=A1=A2=GND → 0x20)
constexpr int I2C_ADDR_XL9535 = 0x20;

// XL9535 registers
constexpr uint8_t XL9535_REG_INPUT0  = 0x00;  // Port 0 (P00-P07) input
constexpr uint8_t XL9535_REG_INPUT1  = 0x01;  // Port 1 (P10-P17) input
constexpr uint8_t XL9535_REG_OUTPUT0 = 0x02;
constexpr uint8_t XL9535_REG_OUTPUT1 = 0x03;
constexpr uint8_t XL9535_REG_CONFIG0 = 0x06;  // 1 = input, 0 = output
constexpr uint8_t XL9535_REG_CONFIG1 = 0x07;

// XL9535 Port 0 bit assignments
constexpr uint8_t XL9535_P0_LCD_BLK = 1 << 0;  // P00 — backlight (output)
constexpr uint8_t XL9535_P0_TS_RST  = 1 << 1;  // P01 — touch reset (output)
constexpr uint8_t XL9535_P0_CAM_RST = 1 << 2;  // P02 — camera reset (output)
constexpr uint8_t XL9535_P0_SE_RST  = 1 << 3;  // P03 — SE050 reset (output)
constexpr uint8_t XL9535_P0_SW2     = 1 << 4;  // P04 — SW2 (input, active-LOW)
constexpr uint8_t XL9535_P0_PB1     = 1 << 5;  // P05 — PB1 (input, active-LOW)
constexpr uint8_t XL9535_P0_PB2     = 1 << 6;  // P06 — PB2 (input, active-LOW)
constexpr uint8_t XL9535_P0_EXT_P1  = 1 << 7;  // P07 — external P1

// XL9535 Port 1 bit assignments
constexpr uint8_t XL9535_P1_EXT_P6  = 1 << 0;  // P10 — external P6
constexpr uint8_t XL9535_P1_SW3     = 1 << 1;  // P11 — SW3 (input, active-LOW, shared ext P7)
constexpr uint8_t XL9535_P1_SW1     = 1 << 2;  // P12 — SW1 (input, active-LOW)
constexpr uint8_t XL9535_P1_EXT_P5  = 1 << 3;  // P13 — external P5
constexpr uint8_t XL9535_P1_EXT_P4  = 1 << 4;  // P14 — external P4
constexpr uint8_t XL9535_P1_EXT_P3  = 1 << 5;  // P15 — external P3
constexpr uint8_t XL9535_P1_EXT_P2  = 1 << 6;  // P16 — external P2
constexpr uint8_t XL9535_P1_IND_LED = 1 << 7;  // P17 — indicator LED (output)

// Button mapping untuk IKeyMap (button_id → XL9535 bit)
// 3 tombol utama: SW1 (kiri), PB1 (tengah), SW2 (kanan)
// PB2 dan SW3 reserved / optional
constexpr uint8_t BTN_LEFT   = 0;  // SW1 → P12 (Port 1, bit 2)
constexpr uint8_t BTN_MIDDLE = 1;  // PB1 → P05 (Port 0, bit 5)
constexpr uint8_t BTN_RIGHT  = 2;  // SW2 → P04 (Port 0, bit 4)
// BTN_PB2 = 3 (P06), BTN_SW3 = 4 (P11) — registered tapi unmapped ke action default

} // namespace kairo::skyrizze32
```

**Catatan penetapan 3 tombol:** `SW1/PB1/SW2` dipilih sebagai tombol navigasi utama
berdasarkan posisi fisik (Kiri/Tengah/Kanan) — dikonfirmasi saat hardware bring-up.
Jika layout fisik berbeda, cukup ubah konstanta `BTN_LEFT/MIDDLE/RIGHT` tanpa
mengubah keymap logic.

---

## XL9535 Driver

XL9535 adalah I²C GPIO expander 16-bit. Berbeda dari TCA9534 (8-bit) di dev board 0:
- 2 port (Port 0: P00-P07, Port 1: P10-P17)
- Register CONFIG terpisah per port (0x06, 0x07)
- 16-bit baca sekaligus (2 byte read input0 + input1)
- INT# pull-low saat ada perubahan; cleared dengan membaca input register

```cpp
class Xl9535 : public IService {
public:
    void init(Runtime& rt);
    void start() override;  // Wire.begin (shared), config port directions, enable INT
    void stop()  override;

    // Output control
    void setOutput(uint8_t port, uint8_t bit, bool high);
    void setBacklight(bool on);                      // P00
    void setIndicatorLed(bool on);                   // P17

    // Input read (16-bit, atomic)
    uint16_t readInputs();   // [port1 << 8 | port0], active-HIGH (inverted from LOW)

    // Interrupt-driven update (called from GPIO43 ISR)
    void onInterrupt();      // set flag; polled in tick() to avoid I2C in ISR context

    void tick(uint64_t nowMs) override;  // drain interrupt flag, emit to keymap

private:
    bool    intFlag_    = false;
    uint16_t lastInputs_ = 0;
    Runtime* rt_ = nullptr;
};
```

### Port configuration (saat start)

```cpp
// Port 0: P00-P03 = output (backlight, resets); P04-P07 = input (SW2, PB1, PB2, ext)
Wire.write(XL9535_REG_CONFIG0, 0b11110000);  // 1=input 0=output

// Port 1: P10 = input (ext P6); P11 = input (SW3); P12 = input (SW1);
//         P13-P16 = input (ext); P17 = output (IND LED)
Wire.write(XL9535_REG_CONFIG1, 0b01111111);  // P17=output, rest=input

// Initial outputs: backlight ON, resets HIGH (deasserted)
setOutput(0, 0, true);   // LCD backlight ON
setOutput(0, 1, true);   // TS_RST deasserted
setOutput(0, 2, true);   // CAM_RST deasserted
setOutput(0, 3, true);   // SE_RST deasserted
setOutput(1, 7, false);  // IND LED off
```

---

## 3-Button Keymap (Plan 27 IKeyMap)

Implementasi `IKeyMap` untuk SkyRizz E32. Semua kerumitan gesture terkurung di sini.

### Mapping

| Tombol | Button ID | Short press | Long press | Gesture lain |
|---|---|---|---|---|
| Kiri (SW1) | 0 | `Prev` | `AdjustDown` | — |
| Tengah (PB1) | 1 | `Activate` | `Back` | — |
| Kanan (SW2) | 2 | `Next` | `AdjustUp` | — |

**Floor guarantee terpenuhi:**
- `Prev` → SW1 short ✓
- `Next` → SW2 short ✓
- `Activate` → PB1 short ✓
- `Back` → PB1 long ✓

**Capabilities board declare:**
```cpp
rt.capabilities().add("input.prev");
rt.capabilities().add("input.next");
rt.capabilities().add("input.activate");
rt.capabilities().add("input.back");
rt.capabilities().add("input.adjust");   // via long-press L/R
// TIDAK add "input.2d" — tidak ada Up/Down/Left/Right native
```

Screen query `has("input.2d")` → false → VirtualKeyboard render mode 1D.

### Hint labels

```cpp
const char* E32KeyMap::hintFor(Action a) const {
    switch (a) {
        case Action::Prev:       return "◀";
        case Action::Next:       return "▶";
        case Action::Activate:   return "●";
        case Action::Back:       return "Hold ●";
        case Action::AdjustUp:   return "Hold ▶";
        case Action::AdjustDown: return "Hold ◀";
        default: return "";
    }
}
```

---

## LCD Display Driver

### Situasi

FPC1 menghubungkan TFT LCD panel via 4-wire SPI. Controller LCD belum dikonfirmasi
dari pin map — ditentukan saat bring-up hardware (kemungkinan ST7789 / ILI9341 /
GC9A01 tergantung panel yang dipasang).

### IDisplayDriver implementation

```cpp
class LcdDriver : public IDisplayDriver {
public:
    void init(Runtime& rt);
    void start() override;  // SPI init, LCD reset sequence, set orientation + color mode

    // IDisplayDriver interface
    uint16_t width()  const override { return width_;  }   // dikonfigurasi sesuai panel
    uint16_t height() const override { return height_; }

    void drawPixel(uint16_t x, uint16_t y, bool on) override;
    void fillRect (uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) override;
    void flush    () override;   // blit framebuffer ke LCD via DMA SPI

    void setBacklight(bool on);  // delegate ke Xl9535::setBacklight()

private:
    uint16_t width_  = 0;   // set saat init dari config/probe
    uint16_t height_ = 0;
    spi_device_handle_t spi_ = nullptr;
    uint8_t* framebuf_ = nullptr;  // 1-bit monochrome framebuffer
    Xl9535*  expander_ = nullptr;  // untuk backlight
};
```

**Framebuffer:** 1-bit monochrome (sama dengan e-ink dev board). Canvas layer
tidak berubah; flush di TFT bisa jauh lebih cepat (< 50ms vs e-ink detik-an).

**Resolusi:** dikonfigurasi via `display/width` + `display/height` di Config Store,
atau di-probe dari panel ID register saat init. Plan 25 (Adaptive UI) handle
layout adjustment otomatis.

**SPI config:**
```cpp
spi_bus_config_t buscfg = {
    .mosi_io_num = PIN_LCD_MOSI,  // GPIO21
    .miso_io_num = -1,             // not connected
    .sclk_io_num = PIN_LCD_SCLK,  // GPIO12
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = width_ * height_ / 8,  // 1-bit framebuffer
};
// CS: GPIO14, DC: GPIO13 — handled in transaction
```

---

## SkyRizz E32 Board class

```cpp
class SkyRizzE32 : public IBoard {
public:
    const char* name() const override { return "skyrizz-e32"; }

    void describeHardware(Runtime& rt) override {
        rt.hardware().add({"display", DriverKind::Display, "TFT LCD (FPC1 SPI)"});
        rt.hardware().add({"wifi",    DriverKind::Wifi,    "ESP32-S3 built-in"});
        rt.hardware().add({"buttons", DriverKind::Other,   "XL9535 3-button (SW1/PB1/SW2)"});
        rt.hardware().add({"rgb",     DriverKind::Other,   "WS2812 x2 GPIO46"});
        rt.hardware().add({"sensors", DriverKind::Other,   "AHT20, LTR-303ALS, SC7A20"});

        rt.capabilities().add("display");
        rt.capabilities().add("wifi");
        rt.capabilities().add("networking");
        rt.capabilities().add("input");
        rt.capabilities().add("input.prev");
        rt.capabilities().add("input.next");
        rt.capabilities().add("input.activate");
        rt.capabilities().add("input.back");
        rt.capabilities().add("input.adjust");
        rt.capabilities().add("rgb");
        rt.capabilities().add("sensors.environment");
        rt.capabilities().add("sensors.light");
        rt.capabilities().add("sensors.motion");
    }

    void registerServices(Runtime& rt) override {
        // I²C init (shared bus)
        Wire.begin(PIN_SDA, PIN_SCL);
        Wire.setClock(400000);  // 400kHz — cukup untuk XL9535 + sensors

        // XL9535 expander (backlight + buttons + resets)
        rt.services().add<Xl9535>();

        // LCD display
        rt.services().add<LcdDriver>();

        // 3-button keymap (Plan 27)
        rt.input().setKeyMap(std::make_unique<E32KeyMap>(rt.services().get<Xl9535>()));

        // Config Store (NVS — sudah dikonfigurasi partition table)
        rt.services().add<NvsConfigStore>();

        // WiFi (existing driver dari Plan 20)
        rt.services().add<Esp32WifiDriver>();
    }
};
```

---

## VirtualKeyboard — mode 1D

Karena board ini tidak punya `input.2d`, VirtualKeyboard perlu mode navigasi linear.
Screen query capability dan memilih render mode:

```cpp
void VirtualKeyboard::onAction(InputAction a) {
    bool is2D = rt_.capabilities().has("input.2d");

    if (is2D) {
        // Existing 2D grid navigation (Up/Down/Left/Right)
        handle2D(a.action);
    } else {
        // 1D: cursor linear menyusuri semua key
        handle1D(a.action);
    }
}

void VirtualKeyboard::handle1D(Action a) {
    switch (a) {
        case Action::Prev: cursorLinear_--; break;
        case Action::Next: cursorLinear_++; break;
        case Action::Activate: typeCurrentKey(); break;
        case Action::Back: if (len > 0) backspace(); else cancel_ = true; break;
        default: break;
    }
    cursorLinear_ = clamp(cursorLinear_, 0, TOTAL_KEYS - 1);
}
```

Render mode 1D: highlight key aktif di posisi `cursorLinear_` (cursor linear
menyusuri baris per baris), bukan cursor 2D `(row, col)`.

---

## Sensor drivers (ringan, out-of-scope fungsional utama)

Sensor AHT20, LTR-303ALS, SC7A20 ada di shared I2C. Driver minimal dibuat tapi
data exposure ke app melalui EventBus (`events::BatteryChanged` → extend ke
`events::SensorReading`). Scope Plan 28 = init + baca basic; app yang consume
data sensor = plan terpisah.

---

## Touch (TSC2007) — deferred Plan 29

TSC2007 terhubung via I2C + PENIRQ di GPIO2. Karena:
1. Resistif touch butuh kalibrasi (4-point min)
2. Screen saat ini tidak support pointer input
3. Input abstraction (Plan 27) sudah extensible untuk `touch.*` custom code

→ Dibuat sebagai **Plan 29 — Touch Input** terpisah. Plan 28 cukup: init TSC2007
(reset via XL9535 P01), baca status, tapi tidak register ke InputRegistry dulu.

---

## File structure

```
firmware/boards/skyrizz-e32/
├─ include/kairo/skyrizze32/
│  ├─ board_config.h        # pin + XL9535 bit constants
│  ├─ skyrizz_e32.h         # SkyRizzE32 : IBoard
│  ├─ xl9535.h              # Xl9535 : IService
│  ├─ lcd_driver.h          # LcdDriver : IDisplayDriver
│  └─ e32_key_map.h         # E32KeyMap : IKeyMap (Plan 27)
├─ src/
│  ├─ skyrizz_e32.cpp
│  ├─ xl9535.cpp
│  ├─ lcd_driver.cpp        # SPI init + framebuffer flush
│  └─ e32_key_map.cpp       # gesture engine + Action mapping
└─ CMakeLists.txt           # idf_component_register
```

Existing yang dipakai tanpa perubahan:
- `firmware/platforms/esp32/` — Esp32Platform
- `firmware/core/` — semua screen, GuiService, Canvas (Plan 25 sudah handle resolusi)
- `firmware/core/src/services/wifi_driver_esp32.cpp` — WiFi (Plan 20)
- `firmware/core/src/services/nvs_config_store.cpp` — NVS (Plan 24)

---

## Tasks

### XL9535 Driver
- [ ] `board_config.h` — semua konstanta dari pin map doc
- [ ] `xl9535.h/cpp` — init (port direction), readInputs (16-bit), setOutput, setBacklight, setIndicatorLed
- [ ] Interrupt handling: GPIO43 ISR set flag; tick() drain + emit ke keymap
- [ ] Test: backlight toggle, indicator LED, baca state SW1/PB1/SW2

### 3-Button Keymap (Plan 27 dependency)
- [ ] `e32_key_map.h/cpp` — `E32KeyMap : IKeyMap`
- [ ] Gesture engine config: short < 500ms, long ≥ 500ms
- [ ] Mapping: SW1=Prev(short)+AdjustDown(long), PB1=Activate(short)+Back(long), SW2=Next(short)+AdjustUp(long)
- [ ] `hintFor()` labels (◀ ▶ ● "Hold ●")
- [ ] `validateFloor()` → pass
- [ ] Baca long_ms dari Config Store (default 500)

### LCD Driver
- [ ] `lcd_driver.h/cpp` — SPI init (esp-idf spi_master), detect/configure panel controller
- [ ] Framebuffer 1-bit monochrome + DMA flush
- [ ] `drawPixel()` + `fillRect()` → framebuffer write
- [ ] Backlight control via Xl9535
- [ ] Verifikasi: Canvas render HomeScreen → tampil di LCD

### Board registration
- [ ] `skyrizz_e32.h/cpp` — `SkyRizzE32 : IBoard`
- [ ] `describeHardware()` + `registerServices()`
- [ ] Wire up ke Esp32Platform board selection (compile-time flag `-DKAIRO_BOARD=skyrizz-e32`)

### VirtualKeyboard 1D mode
- [ ] `virtual_keyboard.cpp` — `handle1D()` + `render1D()` cursor linear
- [ ] Query `rt_.capabilities().has("input.2d")` → pilih mode
- [ ] Test: WiFi password entry berfungsi di board 3-tombol

### Sensor init (minimal)
- [ ] AHT20 init (I2C 0x38) — baca temperature + humidity, publish EventBus
- [ ] LTR-303ALS init (I2C 0x29) — baca ambient light
- [ ] SC7A20 init (I2C 0x19) — baca accelerometer raw

### Integration
- [ ] Build clean untuk target `skyrizz-e32`
- [ ] Flash ke hardware, verifikasi boot sequence
- [ ] Tekan 3 tombol → HomeScreen navigasi (Prev/Next/Activate) berfungsi
- [ ] Long-press tengah → Back (keluar dari submenu)
- [ ] LCD backlight on saat active, off saat DPM sleep

---

## Acceptance criteria

- [ ] Build clean: `idf.py -DKAIRO_BOARD=skyrizz-e32 build` tanpa error/warning
- [ ] XL9535: readInputs() mengembalikan state benar untuk SW1/PB1/SW2
- [ ] 3 tombol: HomeScreen navigasi Up/Down/Select berfungsi via Prev/Next/Activate
- [ ] Back: long-press PB1 ≥ 500ms → `Action::Back` → `ViewDispatcher::pop()`
- [ ] `E32KeyMap::validateFloor()` → true (4 floor action terjangkau)
- [ ] LCD: HomeScreen render tampil di layar fisik
- [ ] LCD backlight: on saat active, off setelah DPM sleep timeout
- [ ] VirtualKeyboard: entry WiFi password via 3-tombol berfungsi (mode 1D)
- [ ] Controls screen (Plan 27): tampil board name "skyrizz-e32", hints ◀/▶/●/"Hold ●"
- [ ] `capabilities().has("input.2d")` → false; `has("input.adjust")` → true
- [ ] AHT20 baca temperature tanpa crash; data di log

---

## Risks / notes

- **LCD controller unknown** — tipe panel (ST7789/ILI9341/lainnya) dikonfirmasi
  saat hardware bring-up. Siapkan abstraksi ringan (panel_init() yang bisa di-swap)
  untuk ganti controller tanpa ubah LcdDriver.
- **TFT vs e-ink refresh** — TFT bisa full-refresh setiap frame (< 20ms).
  `GuiService` tidak berubah, tapi `takeRedraw()` boleh lebih agresif (redraw setiap
  tick jika ada animasi di masa depan).
- **Wire shared bus** — XL9535, AHT20, LTR-303ALS, SC7A20, TSC2007, SE050 semua
  di satu I2C. Kecepatan maksimum dibatasi device paling lambat. 400kHz aman untuk
  semua kecuali jika SE050 butuh lebih lambat (cek datasheet saat bring-up).
- **GPIO43 / UART0 TXD** — dipakai sebagai XL9535 INT#. Log ESP-IDF via UART0 tidak
  bisa dipakai bersamaan tanpa konflik; alihkan log ke USB CDC (`esp_console` USB)
  atau matikan log production.
- **GPIO44 / UART0 RXD** — dipakai sebagai SCLK3 (SPI microSD). UART0 RX tidak
  tersedia; ini by design di board ini.
- **3-tombol layout dikonfirmasi hardware** — SW1=kiri, PB1=tengah, SW2=kanan adalah
  asumsi posisi fisik. Verifikasi saat bring-up; jika beda, cukup swap konstanta
  `BTN_LEFT/MIDDLE/RIGHT`.
