# 17 — Kairo Dev Board (ESP32-S3 + e-ink)

> Board layer tier 2 (overview §0): **Kairo Dev Board** (`dev-board`) — hardware testing sementara. Secara fisik = ESP32-S3-WROOM-1 + e-ink 2.7" 264×176 + 6 tombol TCA9534, yaitu device bekas projek sebelumnya yang dimiliki developer. Pinout = single source of truth dari ref `kairo-test-concept-esp32-s3-wroom-1-eink/firmware/main/badge_pins.h`. Saat Kairo Board V1 (PCB custom) jadi, cukup tambah board layer baru — platform `esp32` & Core tidak berubah.

- Status: ☐ Not started
- Milestone: M6 (ESP32 Dev Hardware)
- Depends on: 16 (ESP32 Platform), 08 (registries), 14 (Key/ViewDispatcher)
- Blocks: 18 (e-ink display driver)

---

## Goal

- `DevBoard : IBoard` (`name() == "dev-board"`) mendeklarasikan hardware Kairo Dev Board.
- `board_config.h` — pin map persis dari `badge_pins.h` (single source of truth).
- **Button driver** TCA9534 (6 tombol) → map ke `Key` enum → `ViewDispatcher::handleKey()`.
- Mengisi HardwareRegistry + CapabilityRegistry (display, wifi, buttons, battery).

## Scope

### In scope

- `board_config.h`: semua pin (power gating, I²C, e-ink SPI, button IRQ).
- `DevBoard : IBoard` + `describeHardware()`.
- `TCA9534Buttons` — driver tombol via I²C (Arduino Wire), rising-edge → `Key`.
- Mapping bit→Key (LEFT/DOWN/UP/RIGHT/SELECT/CANCEL).
- Power rail enable (PIN_PWR) + I²C init di platform startup.

### Out of scope

- E-ink display driver (plan 18).
- ATECC608B secure element, sub-GHz, audio.
- Kairo Board V1 (produksi).

---

## Pin Map (dari badge_pins.h — verified)

| Fungsi | GPIO | Catatan |
|---|---|---|
| PWR (peripheral rail) | 18 | HIGH = VCC peripheral ON |
| SE_EN (secure element) | 8 | HIGH = ATECC608B enabled |
| I²C SCL | 9 | TCA9534 + ATECC608B |
| I²C SDA | 10 | |
| BTN_IRQ (TCA9534 INT) | 1 | active LOW, falling edge on press |
| EPD SCK | 11 | SPI clock |
| EPD MOSI | 17 | SPI data (MISO unused = -1) |
| EPD CS | 12 | active LOW |
| EPD DC | 13 | LOW=command, HIGH=data |
| EPD RST | 14 | active LOW reset |
| EPD BUSY | 21 | HIGH saat refresh |

**I²C addresses**: TCA9534 = `0x20`, ATECC608B = `0x60`.

**TCA9534 registers**: INPUT = `0x00`, CONFIG = `0x03`.

**Button bit masks** (active-LOW di expander):

| Bit | Mask | Tombol | Key enum |
|---|---|---|---|
| 0 | `1<<0` | LEFT | `Key::Left` |
| 1 | `1<<1` | DOWN | `Key::Down` |
| 2 | `1<<2` | UP | `Key::Up` |
| 3 | `1<<3` | RIGHT | `Key::Right` |
| 4 | `1<<4` | SELECT | `Key::Select` |
| 5 | `1<<5` | CANCEL | `Key::Cancel` |

---

## Design

### File

```text
firmware/boards/dev-board/
├─ include/kairo/devboard/
│  ├─ board_config.h       # pin constants (mirror badge_pins.h dari ref)
│  └─ dev_board.h
├─ src/
│  ├─ dev_board.cpp
│  └─ tca9534_buttons.cpp
└─ CMakeLists.txt          # idf_component_register
```

### board_config.h

```cpp
#pragma once
namespace kairo::devboard {
// Power
constexpr int PIN_PWR    = 18;
constexpr int PIN_SE_EN  = 8;
// I²C
constexpr int PIN_SCL    = 9;
constexpr int PIN_SDA    = 10;
constexpr int PIN_BTN_IRQ= 1;
// E-ink SPI
constexpr int PIN_EPD_SCK  = 11;
constexpr int PIN_EPD_MOSI = 17;
constexpr int PIN_EPD_CS   = 12;
constexpr int PIN_EPD_DC   = 13;
constexpr int PIN_EPD_RST  = 14;
constexpr int PIN_EPD_BUSY = 21;
// I²C addrs / regs
constexpr int I2C_TCA9534      = 0x20;
constexpr int TCA9534_REG_INPUT  = 0x00;
constexpr int TCA9534_REG_CONFIG = 0x03;
// Button bits
constexpr uint8_t BTN_LEFT   = 1 << 0;
constexpr uint8_t BTN_DOWN   = 1 << 1;
constexpr uint8_t BTN_UP     = 1 << 2;
constexpr uint8_t BTN_RIGHT  = 1 << 3;
constexpr uint8_t BTN_SELECT = 1 << 4;
constexpr uint8_t BTN_CANCEL = 1 << 5;
}
```

### TCA9534Buttons (IService)

Polling rising-edge — sama pola dengan ref `read_buttons()`:

```cpp
class TCA9534Buttons : public IService {
public:
    void init(Runtime& rt);
    void start() override;  // Wire.begin + config TCA9534 all-input
    void stop()  override;
    void tick(uint64_t nowMs) override;  // poll @ ~50ms, rising-edge → handleKey
private:
    uint8_t read();                  // (~Wire.read()) & 0x3F
    Key     bitToKey(uint8_t mask);  // LEFT→Left, ... CANCEL→Cancel
    Runtime* rt_ = nullptr;
    uint8_t  last_ = 0;
    uint64_t lastPoll_ = 0;
};

void TCA9534Buttons::tick(uint64_t now) {
    if (now - lastPoll_ < 50) return;
    lastPoll_ = now;
    uint8_t btns = read();
    uint8_t pressed = btns & ~last_;   // rising edge
    last_ = btns;
    for (uint8_t b = 0; b < 6; b++) {
        if (pressed & (1 << b)) {
            rt_->view().handleKey(bitToKey(1 << b));
            rt_->view().requestRedraw();
        }
    }
}
```

`bitToKey` mengikuti tabel di atas — menghasilkan `Key` enum yang **sama persis** dengan yang dipakai simulator. Inilah inti "tombol konsisten di board & simulator": satu `Key` enum, dua sumber input (TCA9534 vs stdin).

### DevBoard

```cpp
void DevBoard::describeHardware(Runtime& rt) {
    rt.hardware().add({"display", DriverKind::Display, "e-ink 264x176 GDEY027T91"});
    rt.hardware().add({"wifi",    DriverKind::Wifi,    "ESP32-S3 built-in"});
    rt.hardware().add({"buttons", DriverKind::Other,   "TCA9534 6-button"});

    rt.capabilities().add("display");
    rt.capabilities().add("wifi");
    rt.capabilities().add("networking");
    rt.capabilities().add("input");
}
```

### Power & I²C init

Di `Esp32Platform` atau `DevBoard` (platform startup):
```cpp
pinMode(PIN_PWR, OUTPUT);   digitalWrite(PIN_PWR, HIGH);
pinMode(PIN_SE_EN, OUTPUT); digitalWrite(PIN_SE_EN, HIGH);
delay(50);
Wire.begin(PIN_SDA, PIN_SCL); Wire.setClock(100000);
```

---

## Tasks

- [ ] `board_config.h` (mirror badge_pins.h).
- [ ] `DevBoard` + `describeHardware()`.
- [ ] `TCA9534Buttons` (Wire init, poll, rising-edge → Key → ViewDispatcher).
- [ ] `bitToKey()` mapping sesuai tabel.
- [ ] Power rail + I²C init di platform startup.
- [ ] CMakeLists (idf_component_register, REQUIRES espressif__arduino-esp32).
- [ ] Verifikasi: tekan tombol fisik → screen bereaksi (sama seperti D-pad simulator).

## Acceptance criteria

- 6 tombol terbaca; tekan UP/DOWN menggerakkan cursor HomeScreen (sama dengan simulator).
- SELECT → action; CANCEL → back.
- `Key` enum identik dengan simulator (tidak ada cabang `#ifdef ESP32` di screen logic).
- `capabilities.has("input")` → true.

## Risks / notes

- TCA9534 INT (GPIO1) bisa dipakai untuk interrupt-driven (hemat daya) nanti; MVP cukup polling 50ms di tick.
- Wire (Arduino) dipakai untuk reuse pola ref. Bisa diganti ESP-IDF native i2c_master nanti kalau mau lepas dari arduino-esp32.
- Debounce: rising-edge detection sudah cukup untuk e-ink UX (tidak ada repeat-rate). Tambah debounce kalau perlu.
