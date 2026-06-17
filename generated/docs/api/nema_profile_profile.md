# nema:profile/profile

> gated: `profile`  
> Package: `nema:profile@1.0`  

Owner identity, gated by the "profile" capability (Plan 42). Maps to ProfileService (services/profile_service.h:19–44).

## Functions

| Function | Returns | Flags |
|---|---|---|
| `user-name() → string` | `string` | — |
| `device-name() → string` | `string` | — |
| `has-password() → bool` | `bool` | — |
| `verify-password(input: string) → bool` | `bool` | — |

### `user-name`

The device owner's display name (e.g. "Alice").

**Returns:** `string`

### `device-name`

The user-assigned device name (e.g. "My Palanu").

**Returns:** `string`

### `has-password`

Whether a password/PIN has been set.

**Returns:** `bool`

### `verify-password`

Verify a password/PIN candidate. Uses constant-time comparison. Returns false if no password is set.

**Parameters:**

- `input`: `string`

**Returns:** `bool`
