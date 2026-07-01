# LED subsystem

> Board-agnostic, multi-instance RGB/mono LEDs: `rt.led()` registers each physical
> LED, drives solid/blink effects non-blocking, and maps notification *intents*
> (Working/Success/Error) to whatever the board actually has. Testable in
> Settings → LEDs.

## What it is

Before this, `caps::Rgb` was a declared capability with no HAL, driver, or API —
the WS2812 was never lit. Now there is a full subsystem, modelled on `rt.audio()`.

- **HAL** `ILed` (`firmware/core/include/nema/hal/led.h`): one physical unit (an
  indicator LED or a WS2812 chain of `pixelCount()` pixels) with a `colorModel()`
  of `Rgb` or `Mono`. Dumb by design: `setPixel/setAll/clear/show` + optional
  `setBrightness`. No timing in the driver.
- **Registry + engine** `LedService` = `rt.led()`
  (`firmware/core/src/services/led_service.cpp`): multi-instance (`addLed`, `count`,
  `led(i)`), plus a **non-blocking effect engine** ticked as an adopted service —
  so `blink(...)` keeps running while the app does other work.
- **Driver** `Esp32Ws2812` (`firmware/platforms/esp32/src/esp32_ws2812.cpp`):
  WS2812 chain over the ESP32 RMT peripheral (no external component). Boards
  instantiate it with their pin (skyrizz-solana GPIO2, skyrizz-e32 GPIO46) and
  register it via `rt.led().addLed(...)` in `describeHardware()`.

## API

Low-level (literal control):
```cpp
rt.led().solid(-1, 0, 255, 0);              // all LEDs green (-1 = all, or an index)
rt.led().blink(0, 255, 0, 0, 100, 100, 6);  // LED 0: red, 100ms on/off, 6 cycles
rt.led().off(-1);
```

High-level **notification intents** (board-agnostic — the point):
```cpp
rt.led().notify(LedService::Notify::Working);  // e.g. RFID app while reading
rt.led().notify(LedService::Notify::Success);
rt.led().notify(LedService::Notify::Error);
```
Intents map to a colour+blink profile on RGB, degrade to a blink pattern on mono,
and no-op on a board with no LED — so app code states intent, not hardware. Apps
that need exact colours use `solid()`/`blink()`.

Capabilities: `caps::Led` (has an LED), `caps::LedRgb` (at least one is full RGB);
`caps::Rgb` kept as a legacy alias.

## Settings test

**Settings → LEDs** (gated on `caps::Led`/`caps::Rgb`) lists every LED (pixel
count + RGB/mono) and offers: solid Red/Green/Blue/White, Blink, Off, the three
notification intents, and a Brightness stepper. Multi-instance like Sounds.

## App API (`nema:led`)

Apps drive LEDs via the generated binding (`api/led.pidl` → `nema.led.*`), gated on
`caps::Led`:
```js
nema.led.solid(-1, 0, 255, 0);            // all green (-1 = all)
nema.led.blink(0, 255, 0, 0, 100, 100, 6);
nema.led.notify(1);                        // 1=working 2=success 3=error 4=charging
nema.led.off(-1);
nema.led.brightness(-1, 128);
const labels = nema.led.list();
```
Host impl: `nema_host_impl.cpp` (`led_*`). WASM-app bindings (`wasm_nema.cpp`) are
not wired yet — QuickJS/`.papp` apps have full access.

## Not yet

WASM-app LED bindings; per-pixel app control (current API sets whole LEDs). WS2812
RMT timings are datasheet-correct but HW-unverified. See ADR
[0024](../decisions/0024-hardware-hal-and-settings-test-coverage.md).
