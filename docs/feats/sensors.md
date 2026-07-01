# Sensor subsystem

> Board-agnostic, multi-instance sensors: `rt.sensors()` registers each physical
> sensor with a *specific* type (Light/Motion/Environment/…) and a *generic*
> channel model, so the UI/apps read any sensor without knowing the part. Live in
> Settings → Sensors.

## What it is

Previously `sensors.*` were declared capabilities with no HAL or read path. Now:

- **HAL** `ISensor` (`firmware/core/include/nema/hal/sensor.h`): each sensor has a
  `SensorType` (Environment / Light / Motion / Pressure / Proximity / Other) **and**
  one or more generic **channels** — `channelName(i)` + `channelUnit(i)` + `value(i)`
  (Environment → Temp °C + Humidity %; Motion → X/Y/Z g; Light → lux). `read()`
  samples all channels over I²C; `value(i)` returns the last reading. The generic
  channel model lets the settings UI (and future apps) display any sensor with no
  per-type code, while `type()` keeps it specific enough to map to a capability.
- **Registry** `SensorService` = `rt.sensors()`
  (`firmware/core/src/services/sensor_service.cpp`): multi-instance (`addSensor`,
  `count`, `sensor(i)`). Pure registry — sampling is on demand (no background poll).
- **Drivers** (`firmware/boards/skyrizz-e32/src/e32_sensors.cpp`): `Ltr303` (light,
  0x29) and `Sc7a20` (motion, 0x19). Each **probes on `begin()`**; the board only
  registers a sensor that ACKs. AHT20 (environment) is intentionally not registered
  on E32 — its 0x38 address clashes with the FT6336U touch controller.

## API

```cpp
auto& s = rt.sensors();
for (int i = 0; i < s.count(); i++) {
    ISensor* dev = s.sensor(i);
    dev->read();                                  // I2C sample
    for (int c = 0; c < dev->channelCount(); c++)
        // dev->channelName(c), dev->value(c), dev->channelUnit(c)
}
```

Capabilities stay granular: `caps::SensorsLight`, `caps::SensorsMotion`,
`caps::SensorsEnv` — a board declares one per registered sensor.

## Settings

**Settings → Sensors** (gated on any `sensors.*`) groups each sensor by device and
shows every channel's live value + unit, sampling over I²C at ~2 Hz (via `tick()`,
not every frame).

## Not yet

App-facing PIDL/JS (`nema:sensors`) isn't wired yet — sensors are reachable from
C++ (`rt.sensors()`) and the settings UI. Driver register sequences + scaling
follow datasheet defaults but are HW-unverified; LTR-303 lux is an approximation
(CH0 raw) pending the CH0/CH1 ratio formula. See ADR
[0024](../decisions/0024-hardware-hal-and-settings-test-coverage.md).
