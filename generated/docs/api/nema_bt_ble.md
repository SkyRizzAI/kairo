# nema:bt/ble

> gated: `bt.ble`  
> Package: `nema:bt@1.0`  

BLE controller + GATT peripheral + pairing/bonding. Maps to IBleAdapter (hal/bluetooth.h:36–81).

## Functions

| Function | Returns | Flags |
|---|---|---|
| `enable() → result<tuple<>, string>` | `result<tuple<>, string>` | `@blocking` |
| `disable()` | `void` | — |
| `is-enabled() → bool` | `bool` | — |

### `enable`

Whether the BLE controller is enabled.

**Returns:** `result<tuple<>, string>`

### `disable`

Stop the BLE controller.

### `is-enabled`

Whether BLE is active and advertising/connected.

**Returns:** `bool`
