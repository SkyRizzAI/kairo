# palanu:plp/file_ops

> core (always available)  
> Package: `palanu:plp@1.0`  

File channel opcodes (Channel 0x09).

## Functions

| Function | Returns | Flags |
|---|---|---|
| `list() → u8` | `u8` | — |
| `read() → u8` | `u8` | — |
| `write() → u8` | `u8` | — |
| `mkdir() → u8` | `u8` | — |
| `remove() → u8` | `u8` | — |
| `rename() → u8` | `u8` | — |
| `copy() → u8` | `u8` | — |

### `list`

0x01 — list directory

**Returns:** `u8`

### `read`

0x03 — read file

**Returns:** `u8`

### `write`

0x04 — write file

**Returns:** `u8`

### `mkdir`

0x05 — make directory

**Returns:** `u8`

### `remove`

0x06 — remove file/dir

**Returns:** `u8`

### `rename`

0x07 — rename

**Returns:** `u8`

### `copy`

0x08 — copy

**Returns:** `u8`
