# 66 — USB HID + BadUSB / DuckyScript App

> USB HID keyboard emulation + BadUSB app dengan DuckyScript parser. Fitur
> Flipper Zero yang paling iconic — di Palanu dengan twist: **JS scripting**.

- Status: 🔴 Not started
- Depends on: 34 (Bluetooth/BLE — USB composite), 27 (Input Abstraction),
  42 (Capability Model)
- Blocks: —

---

## 1. Goals

1. USB HID keyboard class di TinyUSB composite device
2. `IUsbHid` HAL interface — send key, send string, combo
3. DuckyScript parser (subset ~30 commands)
4. `BadUsbApp` — pilih script dari `/badusb/`, execute/stop
5. JS BadUSB API: `nema.badusb.type()`, `nema.badusb.combo()`
6. Capability gate: `usb.hid` di manifest `needs[]`

## 2. Arsitektur

### TinyUSB HID Integration

USB composite device saat ini (Plan 34): CDC-ACM. Tambah HID class:

```c
// platforms/esp32/include/tusb_config.h
#define CFG_TUD_HID 1
#define CFG_TUD_HID_EP_BUFSIZE 16
```

HID report descriptor (keyboard boot protocol):

```c
static const uint8_t desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1))
};
```

TinyUSB akan expose composite: CDC-ACM + HID.

### HAL Interface

```cpp
// hal/usb_hid.h
class IUsbHid {
public:
    virtual void sendKey(uint8_t modifier, uint8_t keycode) = 0;
    virtual void sendString(const char* s, uint32_t delayMs = 0) = 0;
    virtual void releaseAll() = 0;
    virtual bool isConnected() = 0;
};
```

### Keycode Mapping

Dari Palanu input `Code` enum (Plan 27) ke USB HID keycode:

```cpp
// core/src/apps/badusb_keymap.cpp
static const std::unordered_map<std::string, uint8_t> kHidKeycode = {
    {"a", HID_KEY_A}, {"b", HID_KEY_B}, /* ... */
    {"ENTER", HID_KEY_ENTER}, {"ESC", HID_KEY_ESCAPE},
    {"TAB", HID_KEY_TAB}, {"SPACE", HID_KEY_SPACE},
    {"UP", HID_KEY_ARROW_UP}, {"DOWN", HID_KEY_ARROW_DOWN},
    {"LEFT", HID_KEY_ARROW_LEFT}, {"RIGHT", HID_KEY_ARROW_RIGHT},
    /* Modifiers handled separately */
};

static const std::unordered_map<std::string, uint8_t> kHidModifier = {
    {"CTRL", KEYBOARD_MODIFIER_LEFTCTRL},
    {"ALT", KEYBOARD_MODIFIER_LEFTALT},
    {"SHIFT", KEYBOARD_MODIFIER_LEFTSHIFT},
    {"GUI", KEYBOARD_MODIFIER_LEFTGUI},
    {"WINDOWS", KEYBOARD_MODIFIER_LEFTGUI},
};
```

### DuckyScript Parser (subset)

DuckyScript 1.0 commands yang didukung:

```
REM             — comment
STRING <text>   — ketik teks
DELAY <ms>      — tunggu ms milidetik
ENTER           — tekan Enter
TAB             — tekan Tab
ESC             — tekan Escape
SPACE           — tekan Space
UP / DOWN / LEFT / RIGHT
CTRL <key>      — Ctrl + key
ALT <key>       — Alt + key
SHIFT <key>     — Shift + key
GUI <key>       — Windows/Command + key
CTRL-ALT <key>  — combo
CTRL-SHIFT <key>
DEFAULTDELAY <ms> — default delay antar command
REPEAT <n>      — ulangi n kali (blok)
```

Parser: line-by-line, split by whitespace, map command → action via lookup
table.

### BadUsbApp

```
┌──────────────────────────────┐
│  BadUSB                       │
│                               │
│  Scripts:                     │
│  ├─ hello-world.dd      ▶    │
│  ├─ open-terminal.dd    ▶    │
│  └─ type-ls.dd          ▶    │
│                               │
│  [Select] Run  [Back] Exit   │
└──────────────────────────────┘
```

- Scan `/badusb/` folder (sama pattern `/apps/` — recursive, `.dd` dan `.txt`)
- Pilih script → tampilkan preview first 5 lines
- Run: eksekusi command-by-command, tampilkan progress bar
- Stop: hold Back atau Cancel — `releaseAll()` + exit

### JS BadUSB API (via nema SDK)

```js
// Plan 48/49 — IDL codegen
import { badusb } from "nema";

// Ketik teks
await badusb.type("Hello World\n");

// Tekan combo
await badusb.combo("CTRL", "ALT", "t");

// Delay
await badusb.delay(500);

// Tekan satu key
await badusb.press("ENTER");
```

**Capability gate:** hanya app dengan `"usb.hid"` di `needs[]` yang dapat
akses `nema.badusb.*`.

### Keamanan

- `usb.hid` capability harus explicit di manifest — tidak auto-grant
- BadUsbApp bawaan punya capability ini; app pihak ketiga harus declare
- User consent: saat pertama kali app minta `usb.hid`, tampilkan dialog
  konfirmasi (opsional — keamanan MVP bisa deferred)

## 3. Implementasi

### Fase 1 — USB HID Foundation (1.5 hari)

1. Enable `CFG_TUD_HID` di TinyUSB config
2. Implementasi `Esp32UsbHid` — TinyUSB HID keyboard send
3. Verifikasi: device muncul sebagai keyboard di host OS (Device Manager /
   System Information)

### Fase 2 — DuckyScript Parser (1 hari)

1. Parser line-by-line
2. Map commands → actions
3. Support subset ~15 commands
4. Unit test dengan sample `.dd` files

### Fase 3 — BadUsbApp UI (1 hari)

1. App list screen — scan `/badusb/`
2. Script preview + run/stop
3. Progress feedback

### Fase 4 — JS API (0.5 hari)

1. `nema.badusb` IDL definition
2. Codegen bridge ke C++ `IUsbHid`
3. Capability gate di JS bridge

## 4. Files

| File | Perubahan |
|------|-----------|
| `firmware/platforms/esp32/include/tusb_config.h` | Enable `CFG_TUD_HID` |
| `firmware/platforms/esp32/src/esp32_usb_hid.cpp` | **Baru** — TinyUSB HID impl |
| `firmware/platforms/esp32/include/nema/esp32/esp32_usb_hid.h` | **Baru** — header |
| `firmware/core/include/nema/hal/usb_hid.h` | **Baru** — IUsbHid interface |
| `firmware/core/src/apps/bad_usb_app.cpp` | **Baru** — BadUsbApp |
| `firmware/core/include/nema/apps/bad_usb_app.h` | **Baru** — header |
| `firmware/core/src/apps/badusb_parser.cpp` | **Baru** — DuckyScript parser |
| `firmware/core/src/apps/badusb_keymap.cpp` | **Baru** — keycode table |
| `packages/nema-sdk/src/idl/badusb.idl` | **Baru** — JS API IDL |
| `/badusb/hello-world.dd` | **Baru** — sample script |

## 5. Acceptance Criteria

- [ ] Device terdeteksi sebagai keyboard HID di macOS/Windows/Linux
- [ ] `STRING Hello World\n` mengetik "Hello World" + Enter di text editor
- [ ] `CTRL ALT t` membuka terminal di Ubuntu / Terminal di macOS
- [ ] `DELAY 1000` jeda 1 detik antar command
- [ ] BadUsbApp bisa pilih script dari `/badusb/`, run, dan stop
- [ ] `nema.badusb.type("test")` berfungsi dari JS app (simulator + hardware)
- [ ] App tanpa `usb.hid` cap tidak bisa akses `nema.badusb`
- [ ] Build hijau: ESP32 (TinyUSB HID + parser)
