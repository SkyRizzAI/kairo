# 0024 — Board-agnostic HAL + settings test coverage for all hardware

- **Status:** accepted
- **Date:** 2026-07-01

## Context

An audit (prompted by the SkyRizz Solana bring-up) found that several declared
capabilities had **no usable API** and **no way to test the hardware**:

- `rgb` (WS2812) — a capability string + a `hardware().add()` line, but no HAL, no
  driver, no runtime accessor. The LEDs were never driven at all.
- `sensors.*` (AHT20/LTR-303/SC7A20) — declared capabilities with the comment
  "data via events in future plans"; no `ISensor`, no read path.
- `battery` — an `IBatteryDriver` existed but was reachable only via a private
  member + events (no accessor, no settings surface).
- `secure.element` — a full HAL, but no settings/status surface or self-test.
- The **Touch settings** screen was an empty stub; the touch-test diagnostic that
  existed pre-migration had been dropped (ADR-adjacent to commit 1230296).

Meanwhile the **Sounds** screen was the gold standard: it enumerates *all* audio
inputs/outputs (multi-instance), shows a live meter per device, and has a "Test
Beep" action. The goal: bring every peripheral up to that bar — discoverable in
Settings, testable, and board-agnostic (driven by capabilities, never board type).

## Decision

1. **Model every peripheral class as a multi-instance registry on the Runtime,
   mirroring `rt.audio()`.** New: `rt.led()` (LedService) and `rt.sensors()`
   (SensorService). A board registers each physical unit (`addLed`/`addSensor`);
   everything downstream enumerates them, so a board can have several LEDs/sensors
   and the UI adapts with no per-board code.

2. **New HALs:**
   - `ILed` (hal/led.h) — per-LED pixel count + colour model (RGB/mono). Dumb:
     set pixels + show(). Timing/patterns live in LedService's non-blocking effect
     engine (solid/blink) so drivers stay trivial and effects are shared.
   - `ISensor` (hal/sensor.h) — a specific `SensorType` (Environment/Light/Motion/…)
     **plus** a generic typed-channel model (name+unit+value per channel), so the
     UI/apps read any sensor without knowing the part. Capabilities stay granular
     (`sensors.light`, `sensors.motion`, …).

3. **Two-layer LED API.** Low-level colour/blink for apps that need it (e.g. an
   RFID reader blinking while scanning), plus board-agnostic **notification
   intents** (`notify(Working/Success/Error/Charging)`) that map to colour+blink
   on RGB, degrade to blink on mono, and no-op with no LED — so app code states
   intent, not hardware specifics.

4. **A settings screen with a hardware test for every peripheral**, capability-
   gated so it appears only where the hardware exists: Touch (restored Touch Test
   app), LEDs (colours/blink/notify/brightness), Sensors (live per-channel values),
   Battery (level/charging), Secure Element (status + Generate-random), Camera
   (acquire-on-use capture test). All follow the Sounds pattern (ComponentScreen +
   MenuBuilder; live values via `tick()`+re-read).

5. **WS2812 driver** lives in the esp32 platform (`Esp32Ws2812`, raw RMT, no extra
   component); boards instantiate it with their pin. Sensor drivers live per-board
   and **probe on init** — only ACK'ing devices are registered.

## Consequences

- `rgb`/`sensors` are no longer dead capabilities: both SkyRizz boards drive their
  WS2812 via `rt.led()`; SkyRizz E32 exposes light + motion sensors.
- Capability additions: `caps::Led`, `caps::LedRgb` (Rgb kept as a legacy alias),
  `caps::Battery`.
- **App-facing APIs (PIDL/JS) for LED + sensors are NOT yet added** — this ADR
  covers the HAL, runtime registries, and on-device settings/tests. Exposing
  `rt.led()`/`rt.sensors()` (and finishing camera/audio-in) to apps is follow-on
  work (media.pidl-style interfaces).
- Sensor drivers + the WS2812 timing are written to datasheet defaults but are
  **HW-unverified** (no hardware on hand); AHT20 is intentionally not registered on
  E32 because its 0x38 address clashes with the FT6336U touch controller.
- Firmware builds green on host + both ESP-IDF targets. See feats
  [`led`](../feats/led.md) and [`sensors`](../feats/sensors.md).
