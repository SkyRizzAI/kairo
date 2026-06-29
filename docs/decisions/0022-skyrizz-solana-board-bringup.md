# 0022 — SkyRizz Solana board bring-up

- **Status:** accepted
- **Date:** 2026-06-29

## Context

A second hardware variant, **SkyRizz Solana** (board id `skyrizz-solana`,
schematic "Lanyard v2"), needs first-class support. It shares the MCU family with
SkyRizz E32 — **ESP32-S3-WROOM-1-N16R8** (16 MB flash, 8 MB Octal PSRAM) — but the
peripheral wiring differs enough that it cannot reuse the E32 board layer verbatim.
Hardware reference: `refs/SkyRizz-Solana-io.html`.

Key differences from SkyRizz E32:

| Concern | SkyRizz E32 | SkyRizz Solana |
|---|---|---|
| I/O expander | XL9535 (16-bit) — buttons **+** backlight + resets | TCA9534 (8-bit) — **buttons only** |
| Buttons | 5 (3 below + 2 side) | 6 (D-pad + OK + Back) on PB1–PB6 (P0–P5) |
| LCD backlight / reset | via XL9535 GPIO | **direct ESP32 GPIO** (BL=7, RST=14) |
| Touch | FT6336U @0x38 (fixed) | FT6336U @0x38 **or** TSC2007 @0x4A (auto-detect) |
| SE050 enable | reset via XL9535 P03 | **direct GPIO8** (SE_EN, HIGH=on) |
| Camera / speaker / mic | GC2145 + NS4168 + ES7243E | none wired (PDM mics share USB pins, parked) |
| microSD | TF1 on SPI3 | none |

The schematic does not label the physical arrangement of PB1–PB6, nor the exact
LCD panel/resolution.

## Decision

1. **New self-contained board layer** `firmware/boards/skyrizz-solana/`
   (namespace `nema::skyrizzsolana`) rather than parameterising the E32 drivers.
   Boards are independent per project convention; the small driver duplication is
   cheaper than coupling two boards through shared, conditionally-compiled code.

2. **TCA9534 is buttons-only.** Because backlight/reset/SE-enable are direct GPIOs
   here, the expander driver carries no output logic — it only reads the six button
   bits (INT on GPIO4, 15 ms poll fallback for held-button repeat).

3. **Button mapping follows the canonical TCA9534-@0x20 bit order documented in
   `ui/key.h`** (bit0=Left, bit1=Down, bit2=Up, bit3=Right, bit4=Select/OK,
   bit5=Cancel/Back) — the same chip and address this board uses. This yields the
   chosen **D-pad + OK + Back** layout and full 2D directional input (`input.2d`).
   With a dedicated Back button, OK is free for tap=Activate / long-hold=Menu (no
   double-tap needed). If a button reads wrong at bring-up, swap the `PB_*` mask↔id
   pairing in `tca9534.cpp::feedButtons()`.

4. **Touch auto-detects.** `TouchPanel` pulses the shared reset (GPIO6) then probes
   0x38 then 0x4A and drives whichever ACKs — capacitive (FT6336U, panel coords) or
   resistive (TSC2007, raw-ADC scaled through a tunable calibration window). One
   service covers either fitment.

5. **SE050C2 reuses the E32 `se05x` nano-package component** (shared via
   `EXTRA_COMPONENT_DIRS` → `../skyrizz-e32/components/se05x`), since its
   `sm_i2c.cpp` already binds to Wire's bus at 0x48. Only the enable path changes
   (direct GPIO8). Mode-B seal self-test still gates `secure.store` (fail-closed).

6. **LCD = ILI9341 240×320** (same controller as E32), so the panel init sequence
   is reused; backlight/reset move to direct GPIO. The dedicated LCDRST line gets a
   hardware reset pulse in `start()`.

## Consequences

- `bun run build:skyrizz-solana` / `flash:skyrizz-solana` build and flash the new
  target; the firmware compiles clean against ESP-IDF v5.5.4 + arduino-esp32 3.3.8.
- WiFi/BLE/PSRAM tuning is identical to E32 (same MCU), so `sdkconfig.defaults` and
  the libnet80211 deauth patch carry over unchanged.
- Bring-up TODOs are explicit in code: button physical assignment, TSC2007
  calibration window, and per-orientation touch mirroring — all isolated to single
  spots, none requiring a redesign.
- Reusing the E32 `se05x` component couples the two targets at the source level:
  moving/renaming `skyrizz-e32/components/se05x` would break the Solana build. If a
  third consumer appears, promote it to `firmware/vendor/` (or `firmware/components/`).
