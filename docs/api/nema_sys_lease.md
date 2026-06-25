# nema:sys/lease

> core (always available)  
> Package: `nema:sys@1.0`  

Resource lease API. Plan 87. Apps acquire an exclusive lease before accessing radio hardware. Host implementation delegated to ResourceBroker (Fase 2).

## Functions

| Function | Returns | Flags |
|---|---|---|
| `acquire(cap: string) → result<u32, lease-error>` | `result<u32, lease-error>` | — |
| `release(lease-handle: u32) → result<tuple<>, string>` | `result<tuple<>, string>` | — |

### `acquire`

Acquire an exclusive lease for a capability. Returns a lease handle on success, or a lease-error if busy/denied.

**Parameters:**

- `cap`: `string`

**Returns:** `result<u32, lease-error>`

### `release`

Release a previously acquired lease.

**Parameters:**

- `lease-handle`: `u32`

**Returns:** `result<tuple<>, string>`
