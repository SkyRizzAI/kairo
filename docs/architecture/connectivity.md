# Connectivity — implementing WiFi/BLE on a new board

> How a board provides networking to Palanu. The core + apps program against the
> HAL contracts (`IWifiDriver`, `IBluetoothController`/`IBleAdapter`); a board only
> writes the concrete driver. Capability-gated, never board-type-gated.
>
> Reference implementations:
> - `firmware/tests/null_wifi_driver.h` — the **smallest conforming** `IWifiDriver`
>   (overrides only the mandatory methods; everything else uses HAL defaults).
> - `firmware/platforms/wasm/src/sim_wifi_driver.cpp` — a fuller RAM-only driver
>   (saved networks, auto-join, state machine, events).
> - `firmware/platforms/esp32/src/esp32_wifi_driver.cpp` — the real one (NVS, static
>   IP, country code, coexistence).

---

## WiFi — `IWifiDriver` (`core/include/nema/hal/wifi.h`)

### Mandatory (pure virtual — a board MUST implement)

| Method | Contract |
|---|---|
| `connect(ssid, pw)` | set state `Connecting`; on success `Connected`, on failure `Failed` + `lastError()`. May block → worker. |
| `disconnect()` | → state `Idle`, `isConnected()==false`. |
| `isConnected()` / `ssid()` | current association. |
| `state()` / `lastError()` | the lifecycle enum (`WifiState`/`WifiError`) the UI shows. **Drive these, not just a bool.** |
| `rssi()` | RSSI of the connected AP (dBm), `0` if none. |
| `scan()` / `scanResults()` | blocking scan (worker), fills the result vector. |
| `ip()` / `ipConfig()` / `setIpConfig()` | IPv4. `ipConfig()` should report the **live** address; `setIpConfig()` applies (and ideally persists) DHCP vs static. |

### Optional (virtual with safe defaults — implement for a "mature" board)

| Method | Default | Implement to get… |
|---|---|---|
| `isEnabled()` / `setEnabled(on)` | derived from `state` / no-op | Wi-Fi on/off radio toggle |
| `isOnline()` | `isConnected()` | a single online gate for HTTP/NTP/remote-net |
| `saveNetwork`/`forgetNetwork`/`savedCount`/`savedAt` | none | the "MY NETWORKS" list + reconnect |
| `connectSaved(ssid)` | `false` | rejoin without re-typing the password |
| `setAutoJoin(ssid,on)` | no-op | per-network auto-join |
| `autoConnect()` | no-op | reconnect-on-boot to the best saved AP |

### Threading contract (Nema)

- `scan()`, `connect()`, `connectSaved()`, `autoConnect()` **may block** → callers run
  them on a `TaskRunner` worker. The `Done` callback (UI thread) reads results.
- Query methods (`state`/`isConnected`/`ssid`/`ip`/`rssi`/`scanResults`/`saved*`) are
  read from the UI thread. Publish transitions via the **`AsyncEventPoster`**
  (`NetworkConnected/Disconnected`, `WifiScanComplete`, `WifiStateChanged`) — never
  touch `EventBus`/`Logger`/`CapabilityRegistry::setState` from a radio/event task.
- Report liveness: `caps.setState(caps::NetWifi, Available|Fault)` from the owner.

A regression in the state machine is caught by `firmware/tests/wifi_contract_test.cpp`.

---

## Bluetooth LE — `IBluetoothController` + `IBleAdapter` (`hal/bluetooth.h`)

`IBluetoothController` owns the radio (`enable`/`disable`/`isEnabled`/`mode`/`address`/
`deviceName`). `IBleAdapter` is the GATT peripheral (`registerService`, advertise,
`notify`/`onWrite`, numeric-comparison pairing `onPairRequest`/`confirmPairing`, bonded
list). The PLP remote layer is one consumer of the adapter; apps may register their own
services. Central role (`startScan`/`connectTo`, Plan 67) is **optional** (default no-op).

- `enable()` inits a heavy stack → call from a worker.
- Report liveness: `caps.setState(caps::BtBle, Available|Fault|Absent)`.
- **sdkconfig (ESP32):** `CONFIG_BT_ENABLED` + `CONFIG_BT_NIMBLE_ENABLED` must be set, and
  `CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y` for WiFi+BLE to run simultaneously on the shared
  2.4 GHz radio. Without these the driver compiles as no-op stubs.

---

## Wiring (platform, not board)

On ESP32 the **platform** (`Esp32Platform::registerDrivers`) registers the WiFi + BLE
drivers and adds the `net.wifi` / `bt.ble` capabilities — so every ESP32 board gets them
for free. A board only declares the *physical* hardware it has; it does not re-implement
WiFi/BLE. Apps then check `capabilities().has("net.wifi")`, never the board name.
