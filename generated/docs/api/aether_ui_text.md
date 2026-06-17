# aether:ui/text

> gated: `null`  
> Package: `aether:ui@1.0`  

Text widgets.

## Functions

| Function | Returns | Flags |
|---|---|---|
| `label(content: string) → handle` | `handle` | — |
| `styled(content: string, variant: string) → handle` | `handle` | — |

### `label`

Simple text label (body style).

**Parameters:**

- `content`: `string`

**Returns:** `handle`

### `styled`

Styled text. variant ∈ {"title", "subtitle", "body", "caption"}.

**Parameters:**

- `content`: `string`
- `variant`: `string`

**Returns:** `handle`
