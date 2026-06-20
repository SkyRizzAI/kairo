# 0001 — Forge remote over USB Serial/JTAG drives HWCDC directly (not Arduino `Serial`)

- **Status:** Accepted
- **Date:** 2026-06-20
- **Area:** esp32/usb (`firmware/platforms/esp32/src/esp32_usb_cdc.cpp`)

## Context

The SkyRizz E32 has two USB modes (see CLAUDE.md "USB mode toggle"):

- **USB HID/CDC mode** (`ARDUINO_USB_CDC_ON_BOOT=1`): `Serial` is the native USB CDC
  (TinyUSB). Used for BadUSB (Plan 66). Flashing needs manual download mode.
- **JTAG/Serial mode** (flag commented out): chosen for fast development flashing — no
  button dance. Here Arduino `Serial` resolves to `HardwareSerial(0)` = **UART0
  (GPIO43/44)**, which on this board is occupied by the XL9535/SPI and **is not wired to
  the host at all**.

After USB HID/CDC was disabled (commit `bddf92f`) to get fast JTAG flashing, the Forge
remote desktop got stuck on "Waiting for device…" forever. The browser sent the PLP
HELLO frame every 300 ms; the device never replied with ACK.

Investigation on hardware established:

- The only host port is `/dev/cu.usbmodem101` — the ESP32-S3 **built-in USB Serial/JTAG
  (HWCDC)**. There is **no USB-UART bridge**, so UART0 is invisible to the host.
- The firmware was reading/writing PLP on Arduino `Serial` = UART0 → bytes went nowhere.
- Root cause was a preprocessor bug: the code selected its transport with
  `#ifdef ARDUINO_USB_CDC_ON_BOOT`. But arduino-esp32's `HardwareSerial.h` does
  `#ifndef ARDUINO_USB_CDC_ON_BOOT / #define ARDUINO_USB_CDC_ON_BOOT 0` — so once
  `<Arduino.h>` is included the macro is **always defined** (to `0` in JTAG mode).
  `#ifdef` was therefore always true and the HWCDC branch never compiled. Verified by
  inspecting the object file: no `HWCDC` symbols were present.

## Decision

In JTAG/Serial mode, drive the **Arduino `HWCDC` class directly** (the built-in USB
Serial/JTAG, which owns its own LL-based ISR) for the PLP link, instead of `Serial`.

Select the branch by the macro's **value**, never its existence:

```cpp
#include <Arduino.h>          // defines ARDUINO_USB_CDC_ON_BOOT (0 in JTAG mode)
#if ARDUINO_USB_CDC_ON_BOOT   // == 1 → USB CDC mode → use Serial (USB CDC)
#else                         // == 0 → JTAG mode    → use HWCDC
#endif
```

Keep `CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=y` so `rt.log()` stays visible in
`idf.py monitor` on the same HWCDC wire. PLP frames and console text share the wire; the
FrameParser (firmware and browser) is noise-tolerant and resyncs on the `0xAB` magic byte.

Verified on hardware: the device ACKs the first HELLO immediately.

## Consequences

- Forge remote works in **both** USB modes; fast JTAG flashing is retained.
- **Load-bearing rule:** any code in `esp32_usb_cdc.cpp` (or similar) that branches on
  `ARDUINO_USB_CDC_ON_BOOT` / `ARDUINO_USB_ON_BOOT` MUST use `#if` (test the value), never
  `#ifdef`/`#ifndef`. The same trap applies to `esp32_usb_hid.cpp`, which is currently safe
  only because it does not include `<Arduino.h>`/`<USB.h>` and so the macro stays undefined
  there.
- Opening the USB Serial/JTAG port resets the chip (auto-reset circuit), so the first ~3-4 s
  after connect are boot time; Forge's periodic HELLO reconnects automatically after boot.
- Side finding (benign, not fixed): `PIN_SD_SCLK = GPIO44` is UART0 RX. Harmless here
  because the host uses HWCDC, not UART0, and SD SCLK only steals an already-unused pin.
