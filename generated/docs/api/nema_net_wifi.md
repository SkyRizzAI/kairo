# nema:net/wifi

> gated: `net.wifi`  
> Package: `nema:net@1.0`  

Wi-Fi station management. Maps to IWifiDriver (hal/wifi.h:30–43). @future — not yet exposed in JS v0 (js_api.cpp); defined here so the IDL is the complete SSOT. Will be added to JS/WASM bindings in Plan 49.

## Functions

| Function | Returns | Flags |
|---|---|---|
| `is-connected() → bool` | `bool` | — |
| `ssid() → string` | `string` | — |
| `ip() → string` | `string` | — |
| `scan() → list<wifi-ap>` | `list<wifi-ap>` | `@blocking` |
| `connect(ssid: string, password: string) → result<tuple<>, string>` | `result<tuple<>, string>` | `@blocking` |
| `disconnect()` | `void` | — |

### `is-connected`

Whether the device is currently connected to an AP.

**Returns:** `bool`

### `ssid`

The SSID of the currently connected AP, or empty string.

**Returns:** `string`

### `ip`

The local IP address, or empty string.

**Returns:** `string`

### `scan`

Start a Wi-Fi scan. @blocking — runs on worker.

**Returns:** `list<wifi-ap>`

### `connect`

Connect to an AP.

**Parameters:**

- `ssid`: `string`
- `password`: `string`

**Returns:** `result<tuple<>, string>`

### `disconnect`

Disconnect from the current AP.
