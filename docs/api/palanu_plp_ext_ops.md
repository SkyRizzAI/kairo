# palanu:plp/ext_ops

> core (always available)  
> Package: `palanu:plp@1.0`  

Ext channel opcodes (Channel 0x06).

## Functions

| Function | Returns | Flags |
|---|---|---|
| `inject_event() → u8` | `u8` | — |
| `wifi_set_networks() → u8` | `u8` | — |
| `app_install() → u8` | `u8` | — |
| `app_scan() → u8` | `u8` | — |

### `inject_event`

0x01 — inject EventBus event

**Returns:** `u8`

### `wifi_set_networks`

0x02 — sim WiFi router config

**Returns:** `u8`

### `app_install`

0x03 — install .papp live

**Returns:** `u8`

### `app_scan`

0x04 — rescan /system/apps/

**Returns:** `u8`
