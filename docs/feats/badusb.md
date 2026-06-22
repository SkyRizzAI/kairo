# BadUSB — Ducky-Script Keystroke Injection

> Plan 66 — The device acts as a USB HID keyboard and replays Ducky scripts into a host
> computer. Requires **USB HID/CDC mode** on SkyRizz E32.

## Overview

`BadUsbApp` is a fullscreen app that lists `.dd` (Ducky) scripts from `/badusb/` on the VFS,
parses them, and replays them as keystrokes over `IUsbHid` into the attached host. This is the
classic "Rubber Ducky" capability.

## Requirements

- **USB HID/CDC mode** must be enabled (SkyRizz E32): uncomment the two `ARDUINO_USB_*` lines in
  `firmware/targets/skyrizz-e32/CMakeLists.txt` and do a clean rebuild. See CLAUDE.md
  "USB mode toggle" and [ADR 0001](../decisions/0001-usb-jtag-remote-uses-hwcdc.md). In JTAG/Serial
  mode TinyUSB is not initialized, so HID is inactive.
- Capability `usb.hid` (declared by `Esp32UsbHid`).

## File reference

| File | Purpose |
|---|---|
| `firmware/core/include/nema/apps/bad_usb_app.h` / `src/apps/bad_usb_app.cpp` | The app: script picker + runner |
| `firmware/core/src/apps/badusb_parser.*` (ns `nema::badusb`) | Ducky-script tokenizer → `vector<Command>` |
| `firmware/core/include/nema/hal/usb_hid.h` | `IUsbHid` interface (`sendKey`/`sendString`/`releaseAll`) |
| `firmware/platforms/esp32/src/esp32_usb_hid.cpp` | ESP32 TinyUSB HID keyboard impl |
| `/badusb/*.dd` (VFS) | Ducky scripts (factory: `demo.dd`, `rickroll_mac.dd`) |

## How it works

`BadUsbApp : ComponentApp` runs on its own thread (Plan 84 Fase 3 migration from `ComponentScreen`). It is installed at boot via a static instance in each target's `main.cpp` and appears in the launcher under **System** category.

```
BadUsbApp launch (own app thread, id = "com.palanu.badusb")
  onStart(ctx)
    ├─ get IFileSystem + IUsbHid from ctx.runtime()
    └─ scan /data/com.palanu.badusb/*.dd (catalogued in scripts_[])
  build(arena, ctx)   → renders script picker list
  onKey(k, ctx)       → select/start/stop; Cancel → app exit (handled by ComponentApp base)
  onTick(ctx)         → execNextCommand() when running (20 ms interval); 0 ms when idle
    ├─ badusb::parse(text) → vector<Command>   (REM/DELAY/STRING/STRINGLN/GUI/ENTER/…)
    └─ for each Command:
         ├─ map named keys → HID codes
         ├─ IUsbHid::sendString / sendKey(modifier, keycode)
         └─ honor DELAY between commands
```

The parser supports the common Ducky subset (REM comment, DELAY ms, STRING, STRINGLN, ENTER, GUI,
modifier combos) with a named-key → HID table. Factory scripts are seeded to `/data/com.palanu.badusb/` on first boot by the ESP32 platform (migrated from `/badusb/` in Plan 83).

## Usage

1. Build/flash in USB HID/CDC mode; plug the device's USB into the target computer.
2. Open the BadUSB app, pick a `.dd` script, run it.
3. To add scripts: drop `.dd` files into `/badusb/` (via the Forge file browser, CLI, or SD card).

## Safety / authorization

BadUSB injects real keystrokes into whatever host it is plugged into. Use only on machines you are
authorized to test. The factory `rickroll_mac.dd` is a harmless demo.

## Extending

- Add Ducky commands in `badusb_parser.cpp` (extend the tokenizer + named-key table).
- The HID layer is platform-abstracted via `IUsbHid`; a new board only needs that impl.
