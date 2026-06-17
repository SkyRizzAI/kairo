# nema:sys/events

> core (always available)  
> Package: `nema:sys@1.0`  

Publish/subscribe event bus. Maps to EventBus (event/event_bus.h). @future — not yet exposed in JS v0 (js_api.cpp); defined here so the IDL is the complete SSOT. Will be added to JS/WASM bindings in Plan 49.

## Functions

| Function | Returns | Flags |
|---|---|---|
| `subscribe(name: string, handler: handle) → handle` | `handle` | — |
| `unsubscribe(token: handle)` | `void` | — |
| `publish(name: string, fields: list<field>)` | `void` | — |

### `subscribe`

Subscribe to an event name (or "*" for all).

**Parameters:**

- `name`: `string`
- `handler`: `handle`

**Returns:** `handle`

### `unsubscribe`

Remove a subscription by handle returned from subscribe.

**Parameters:**

- `token`: `handle`

### `publish`

Publish an event with optional string fields.

**Parameters:**

- `name`: `string`
- `fields`: `list<field>`
