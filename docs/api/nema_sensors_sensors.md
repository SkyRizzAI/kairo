# nema:sensors/sensors

> gated: `sensors`  
> Package: `nema:sensors@1.0`  
## Functions

| Function | Returns | Flags |
|---|---|---|
| `list() → list<string>` | `list<string>` | — |
| `read(index: u32) → list<string>` | `list<string>` | — |

### `list`

List sensor labels (one per registered sensor).

**Returns:** `list<string>`

### `read`

Read one sensor's channels, each formatted "name=value unit" (e.g. "Temp=24.30 C", "X=0.01 g"). `index` is the position in list().

**Parameters:**

- `index`: `u32`

**Returns:** `list<string>`
