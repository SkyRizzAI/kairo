# palanu:plp/channels

> core (always available)  
> Package: `palanu:plp@1.0`  

PLP channel numbers — the wire multiplexer lanes. Each frame carries a channel byte that routes it to the correct handler on both sides. Mirrors firmware: nema::plp::Channel (plp_codec.h) and codec.ts.

## Functions

| Function | Returns | Flags |
|---|---|---|
| `control() → u8` | `u8` | — |
| `screen() → u8` | `u8` | — |
| `input() → u8` | `u8` | — |
| `log() → u8` | `u8` | — |
| `system() → u8` | `u8` | — |
| `ota() → u8` | `u8` | — |
| `ext() → u8` | `u8` | — |
| `event() → u8` | `u8` | — |
| `cli() → u8` | `u8` | — |
| `file() → u8` | `u8` | — |

### `control`

0x00 — HELLO / ACK / REJECT / auth handshake

**Returns:** `u8`

### `screen`

0x01 — framebuffer (1-bit, optionally RLE)

**Returns:** `u8`

### `input`

0x02 — action / pointer

**Returns:** `u8`

### `log`

0x03 — log entries

**Returns:** `u8`

### `system`

0x04 — device info, power (restart/sleep/shutdown)

**Returns:** `u8`

### `ota`

0x05 — firmware update chunks

**Returns:** `u8`

### `ext`

0x06 — host→device sim-control commands

**Returns:** `u8`

### `event`

0x07 — device→host EventBus stream

**Returns:** `u8`

### `cli`

0x08 — terminal: host sends command, device streams text + EOT

**Returns:** `u8`

### `file`

0x09 — filesystem request/response

**Returns:** `u8`
