# palanu:plp/ota_ops

> core (always available)  
> Package: `palanu:plp@1.0`  

OTA channel opcodes (Channel 0x05).

## Functions

| Function | Returns | Flags |
|---|---|---|
| `begin() → u8` | `u8` | — |
| `data() → u8` | `u8` | — |
| `end() → u8` | `u8` | — |

### `begin`

0x01 — start OTA with image size

**Returns:** `u8`

### `data`

0x02 — chunk at offset (idempotent)

**Returns:** `u8`

### `end`

0x03 — finalize upload

**Returns:** `u8`
