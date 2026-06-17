# nema:input/input

> gated: `input`  
> Package: `nema:input@1.0`  

Input funnel: physical buttons → input::Code + input::Action. Maps to InputService (services/input_service.h:24–55).

## Functions

| Function | Returns | Flags |
|---|---|---|
| `hint(action: string) → string` | `string` | — |
| `actions() → list<string>` | `list<string>` | — |

### `hint`

Get the hardware-specific hint label for an Action.

**Parameters:**

- `action`: `string`

**Returns:** `string`

### `actions`

List all supported actions on this device.

**Returns:** `list<string>`
