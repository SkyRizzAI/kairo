# WiFi Connectivity

> Plan 20 / 23 — Scan and join WiFi networks without ever freezing the UI; HTTP fetches and NTP time
> ride on top. Capability-gated (`net.wifi`).

## Overview

WiFi is exposed through `IWifiDriver` (capability `net.wifi`). All blocking radio work (scan ~1-3 s,
connect ~1-3 s) runs on the `TaskRunner` worker thread, so the UI keeps rendering — the
"never-freeze" architecture pillar. On hardware the ESP32 driver is real; in the simulator a virtual
router stands in (Forge can inject networks). Liveness is mirrored into the capability/resource model
so the system knows when WiFi is actually up.

## How it works

```
WiFi UI / app ── TaskRunner::submit(scan/connect work, done) ──▶ worker thread (may block)
                                                                  └─ done() on UI thread
IWifiDriver state changes ──▶ Network connect/disconnect events ──▶ CapabilityRegistry.setState(NetWifi, …)
HTTP / NTP gated on isOnline() so behavior matches hardware honesty
```

- **Liveness**: `NetWifi` seeded `Absent`; connect/disconnect events flip it `Available`/`Absent`.
- **HTTP is gated on WiFi state** (even in the simulator, via `isOnline()`) so apps behave the same
  on device and in the sim.
- **NTP** (`NtpService`) is adopted only when `NetWifi` exists; `syncNtp()` runs on the worker and
  sets the wall clock.

## File reference

| File | Purpose |
|---|---|
| `firmware/core/include/nema/hal/wifi.h` | `IWifiDriver` interface (connect/scan/ip) |
| `firmware/platforms/esp32/src/esp32_wifi_driver.cpp` | ESP32 radio impl |
| `firmware/platforms/wasm/src/sim_wifi_driver.cpp` | Simulator virtual router |
| `firmware/core/include/nema/hal/http_client.h` + `platforms/esp32/src/esp32_http_client.cpp` | HTTP over WiFi (HTTPS, cert bundle) |
| `firmware/core/src/runtime.cpp` | WiFi liveness bridge, NtpService adoption |

## Usage

- **On device**: a WiFi UI (scan → pick → password → connect) runs the radio work on the worker
  thread; the UI stays responsive throughout.
- **In the simulator**: Forge → Simulator → **Settings → WiFi**: define virtual networks (SSID,
  RSSI, password, online toggle) via the `WifiSetNetworks` control op; the sim driver serves them.

## Gotchas

- Never call blocking WiFi/HTTP work on the UI thread — always `TaskRunner::submit`.
- HTTP requires `net.wifi` to be `Available`; apps should check capabilities, not assume.
- ESP32 TLS uses `esp_crt_bundle_attach`; an `insecure` flag skips common-name checks.

## Extending

- A new platform supplies its own `IWifiDriver`; core/app code is unchanged (capability-gated).
- Networked apps (e.g. the Ticker) submit fetches to the worker and update UI in the completion
  callback — see [`../architecture/runtime-kernel.md`](../architecture/runtime-kernel.md) (TaskRunner).
