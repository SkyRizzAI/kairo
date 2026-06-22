# palanu:plp/control_ops

> core (always available)  
> Package: `palanu:plp@1.0`  

Control channel opcodes (Channel 0x00).

## Functions

| Function | Returns | Flags |
|---|---|---|
| `hello() → u8` | `u8` | — |
| `ack() → u8` | `u8` | — |
| `reject() → u8` | `u8` | — |
| `auth_challenge() → u8` | `u8` | — |
| `auth_response() → u8` | `u8` | — |
| `auth_ok() → u8` | `u8` | — |
| `auth_fail() → u8` | `u8` | — |
| `auth_required() → u8` | `u8` | — |

### `hello`

0x01 — host→device: start handshake

**Returns:** `u8`

### `ack`

0x02 — device→host: handshake accepted

**Returns:** `u8`

### `reject`

0x03 — device→host: remote disabled

**Returns:** `u8`

### `auth_challenge`

0x20 — device→host: "salt:nonce" challenge

**Returns:** `u8`

### `auth_response`

0x21 — host→device: HMAC response or token

**Returns:** `u8`

### `auth_ok`

0x22 — device→host: authorized + token

**Returns:** `u8`

### `auth_fail`

0x23 — device→host: bad credentials

**Returns:** `u8`

### `auth_required`

0x24 — device→host: need auth for privileged channel

**Returns:** `u8`
