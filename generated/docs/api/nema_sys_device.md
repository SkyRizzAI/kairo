# nema:sys/device

> core (always available)  
> Package: `nema:sys@1.0`  

Device identity and capability queries. Maps to SystemInfo (system_info.h) + CapabilityRegistry (capability_registry.h).

## Functions

| Function | Returns | Flags |
|---|---|---|
| `name() → string` | `string` | — |
| `caps() → list<string>` | `list<string>` | — |
| `has(cap: string) → bool` | `bool` | — |
| `available(cap: string) → bool` | `bool` | — |

### `name`

Board/product name, e.g. "dev-board", "skyrizz-e32".

**Returns:** `string`

### `caps`

All static capabilities this box was built with (Plan 42).

**Returns:** `list<string>`

### `has`

Static: was this box built able to do X?

**Parameters:**

- `cap`: `string`

**Returns:** `bool`

### `available`

Dynamic: is X up and usable right now? (Plan 42 liveness)

**Parameters:**

- `cap`: `string`

**Returns:** `bool`
