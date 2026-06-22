# aether:ui/view

> gated: `null`  
> Package: `aether:ui@1.0`  

Container and layout functions. Begin/end pattern: view-begin opens a container, subsequent widget calls add children to it, view-end closes and returns to the parent.

## Functions

| Function | Returns | Flags |
|---|---|---|
| `view-begin(direction: string) → handle` | `handle` | — |
| `view-end()` | `void` | — |

### `view-begin`

Begin a container view (row or column flex direction). direction: "row" or "col"

**Parameters:**

- `direction`: `string`

**Returns:** `handle`

### `view-end`

End the current container. Children added after this go to the parent.
