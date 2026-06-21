# nema:storage/fs

> core (always available)  
> Package: `nema:storage@1.0`  

Isolated per-app file storage. Paths are relative names (no slashes). Routes to internal flash or SD card based on user routing preference.

## Functions

| Function | Returns | Flags |
|---|---|---|
| `read-file(name: string) → option<string>` | `option<string>` | — |
| `write-file(name: string, data: string) → bool` | `bool` | — |
| `list-files() → list<string>` | `list<string>` | — |
| `remove-file(name: string) → bool` | `bool` | — |
| `bytes-used() → u64` | `u64` | — |

### `read-file`

Read a file as a UTF-8 string. Returns none if the file does not exist.

**Parameters:**

- `name`: `string`

**Returns:** `option<string>`

### `write-file`

Write (create or overwrite) a file with UTF-8 content.

**Parameters:**

- `name`: `string`
- `data`: `string`

**Returns:** `bool`

### `list-files`

List files in the app's storage directory.

**Returns:** `list<string>`

### `remove-file`

Delete a file. Returns true if it existed.

**Parameters:**

- `name`: `string`

**Returns:** `bool`

### `bytes-used`

Total bytes used across all files for this app (internal + SD).

**Returns:** `u64`
