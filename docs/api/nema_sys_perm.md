# nema:sys/perm

> core (always available)  
> Package: `nema:sys@1.0`  

Permission query/request API. Plan 87. Apps use this to check their own permission status and request grants. Host implementation delegated to PermissionService (Fase 1).

## Functions

| Function | Returns | Flags |
|---|---|---|
| `status(cap: string) → u8` | `u8` | — |
| `request(cap: string) → u8` | `u8` | — |

### `status`

Query permission status for a capability. Returns: 0=not_asked  1=granted  2=denied

**Parameters:**

- `cap`: `string`

**Returns:** `u8`

### `request`

Request permission for a capability. For sensitive capabilities this triggers the Allow/Deny screen. Returns: 1=granted  2=denied

**Parameters:**

- `cap`: `string`

**Returns:** `u8`
