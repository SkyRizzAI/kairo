# nema:storage/kv

> core (always available)  
> Package: `nema:storage@1.0`  

Persistent key-value store, always available (no capability gate). Write operations commit immediately — no explicit flush needed.

## Functions

| Function | Returns | Flags |
|---|---|---|
| `get(key: string) → option<string>` | `option<string>` | — |
| `set(key: string, value: string)` | `void` | — |
| `get-int(key: string) → option<s64>` | `option<s64>` | — |
| `set-int(key: string, value: s64)` | `void` | — |
| `remove(key: string) → bool` | `bool` | — |

### `get`

Read a string value. Absent key → none.

**Parameters:**

- `key`: `string`

**Returns:** `option<string>`

### `set`

Write a string value (commits immediately).

**Parameters:**

- `key`: `string`
- `value`: `string`

### `get-int`

Read a 64-bit int. Absent key → none.

**Parameters:**

- `key`: `string`

**Returns:** `option<s64>`

### `set-int`

Write a 64-bit int (commits immediately).

**Parameters:**

- `key`: `string`
- `value`: `s64`

### `remove`

Delete a key. Returns true if the key existed.

**Parameters:**

- `key`: `string`

**Returns:** `bool`
