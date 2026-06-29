# SkyRizz Solana board

> A second ESP32-S3 handheld variant ("Lanyard v2"): ILI9341 TFT over direct SPI,
> a 6-button D-pad via a TCA9534 expander, auto-detecting capacitive/resistive
> touch, and an SE050C2 secure element — built with `bun run build:skyrizz-solana`.

## What it is

Board id `skyrizz-solana`, namespace `nema::skyrizzsolana`. Same MCU as SkyRizz E32
(**ESP32-S3-WROOM-1-N16R8**, 16 MB flash / 8 MB Octal PSRAM), but a distinct
peripheral layout. Hardware source of truth: `refs/SkyRizz-Solana-io.html`; the pin
map lives in `firmware/boards/skyrizz-solana/include/nema/skyrizzsolana/board_config.h`.

| Subsystem | Wiring | Driver |
|---|---|---|
| Display | ILI9341 240×320, SPI (SCK=11, CS=12, DC=13, RST=14, MOSI=15), backlight GPIO7 | `LcdDriver` (`lcd_driver.cpp`) |
| Buttons | 6 (D-pad + OK + Back) via TCA9534 @0x20, PB1–PB6 on P0–P5, INT=GPIO4 | `Tca9534` + `SolanaKeyMap` |
| Touch | FT6336U @0x38 **or** TSC2007 @0x4A (auto), INT=GPIO5, RST=GPIO6 | `TouchPanel` (`touch_panel.cpp`) |
| Secure element | NXP SE050C2 @0x48, enable GPIO8 | `Se050Driver` (reuses E32 `se05x` nano-pkg) |
| RGB | WS2812 ×2 on GPIO2 | capability only (`caps::Rgb`) |
| Battery | ADC divider on GPIO1 | not yet surfaced |
| PDM mics | SPH0641 ×2 on GPIO19/20 | parked (pins are native USB D−/D+) |

I²C bus: SCL=9, SDA=10 @ 400 kHz (shared by TCA9534, touch, SE050).

## How it works

### Buttons (`Tca9534` → `SolanaKeyMap`)

The TCA9534 carries **only** the six push buttons (unlike the E32's XL9535, which
also drove backlight/resets). `Tca9534::tick()` re-reads the input port on the GPIO4
interrupt and on a 15 ms timer (so a held button still produces long-press/repeat),
inverts the active-LOW bits, and feeds edges to the keymap.

The id↔bit mapping follows the canonical TCA9534-@0x20 order documented in
`ui/key.h` (bit0=Left, bit1=Down, bit2=Up, bit3=Right, bit4=Select/OK,
bit5=Cancel/Back). `SolanaKeyMap` maps Up→Prev, Down→Next, Left/Right→Adjust,
OK→Activate (tap) / Menu (long-hold, two-stage), Back→Back — passing
`validateFloor()` and declaring `input.2d` (four arrows → grid virtual keyboard).
Directional buttons rotate with the display (Plan 92). To re-assign a physically
swapped button, edit the `PB_*` mask↔id pairing in `tca9534.cpp::feedButtons()`.

### Touch (`TouchPanel`, auto-detect)

`start()` pulses the shared reset (GPIO6, direct) then probes 0x38 (FT6336U) and
0x4A (TSC2007), driving whichever ACKs:
- **FT6336U** (capacitive): reads TD_STATUS + touch-point 1, already in panel coords.
- **TSC2007** (resistive): measures Z1 (pressure gate) then X/Y, scaling the 12-bit
  ADC through a tunable calibration window (`TSC_X_MIN/MAX`, `TSC_Y_MIN/MAX`).

Both paths emit pointer events in logical canvas coords via the shared
`toLogical()` rotation transform. The `input.touch` capability is declared; the
driver no-ops if neither controller answered.

### Display / secure element

`LcdDriver` reuses the E32 ILI9341 init sequence and 1-bit-framebuffer/RGB565-flush
model, but backlight (GPIO7) and reset (GPIO14) are direct GPIOs — no expander.
`Se050Driver` reuses the vendored `se05x` nano-package component (shared from
`skyrizz-e32/components/se05x`) for mode-B seed sealing; it only differs from the
E32 driver by enabling the chip via direct GPIO8. The boot-time wrap→unwrap
self-test gates `secure.store` fail-closed (wallet falls back to software on any
failure).

## Build & flash

```bash
bun run build:skyrizz-solana          # → build/palanu-skyrizz-solana.bin
bun run flash:skyrizz-solana [PORT]   # flash + monitor
```

Same MCU as E32, so `sdkconfig.defaults` (PSRAM/WiFi/BLE tuning) and the
libnet80211 deauth patch carry over unchanged.

## Bring-up checklist (unverified on hardware)

- Confirm the physical PB1–PB6 → D-pad/OK/Back assignment; swap masks if needed.
- Confirm the LCD panel really is ILI9341 240×320; adjust `panelInit()` otherwise.
- Calibrate the TSC2007 ADC window (and check per-orientation touch mirroring) if a
  resistive panel is fitted.
- See ADR [0022](../decisions/0022-skyrizz-solana-board-bringup.md) for rationale.
