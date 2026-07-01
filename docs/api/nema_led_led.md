# nema:led/led

> gated: `led`  
> Package: `nema:led@1.0`  
## Functions

| Function | Returns | Flags |
|---|---|---|
| `list() → list<string>` | `list<string>` | — |
| `solid(index: s32, r: u8, g: u8, b: u8)` | `void` | — |
| `blink(index: s32, r: u8, g: u8, b: u8, on-ms: u16, off-ms: u16, cycles: s32)` | `void` | — |
| `off(index: s32)` | `void` | — |
| `notify(intent: u8)` | `void` | — |
| `brightness(index: s32, level: u8)` | `void` | — |

### `list`

List LED labels (one per registered LED / strip).

**Returns:** `list<string>`

### `solid`

Set a solid colour. index -1 = all LEDs. r/g/b are 0..255.

**Parameters:**

- `index`: `s32`
- `r`: `u8`
- `g`: `u8`
- `b`: `u8`

### `blink`

Blink a colour. index -1 = all. on-ms/off-ms per phase; cycles -1 = forever.

**Parameters:**

- `index`: `s32`
- `r`: `u8`
- `g`: `u8`
- `b`: `u8`
- `on-ms`: `u16`
- `off-ms`: `u16`
- `cycles`: `s32`

### `off`

Turn LED(s) off. index -1 = all.

**Parameters:**

- `index`: `s32`

### `notify`

Notification intent (maps to colour+blink; degrades on mono; no-op with no LED): 0=off 1=working 2=success 3=error 4=charging.

**Parameters:**

- `intent`: `u8`

### `brightness`

Global brightness 0..255. index -1 = all.

**Parameters:**

- `index`: `s32`
- `level`: `u8`
