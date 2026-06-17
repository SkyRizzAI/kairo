# nema:media/camera

> gated: `camera`  
> Package: `nema:media@1.0`  

Camera capture. Maps to CameraService (services/camera_service.h:10–14).

## Functions

| Function | Returns | Flags |
|---|---|---|
| `list() → list<string>` | `list<string>` | — |
| `capture() → result<string, string>` | `result<string, string>` | `@blocking` |

### `list`

List available camera devices.

**Returns:** `list<string>`

### `capture`

Capture a frame from the default camera. @blocking — runs on worker.

**Returns:** `result<string, string>`
