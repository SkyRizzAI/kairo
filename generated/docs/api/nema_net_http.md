# nema:net/http

> gated: `net.http`  
> Package: `nema:net@1.0`  

Networked HTTP client. Maps to IHttpClient (hal/http_client.h:16–23). All methods BLOCK and MUST run on a TaskRunner worker — never the UI loop. The host wraps @blocking functions as async (Promise in JS, callback in WASM).

## Functions

| Function | Returns | Flags |
|---|---|---|
| `get(url: string) → result<http-response, string>` | `result<http-response, string>` | `@blocking` |
| `post(url: string, body: string, content-type: string) → result<http-response, string>` | `result<http-response, string>` | `@blocking` |
| `request(method: string, url: string, headers: string, body: string) → result<http-response, string>` | `result<http-response, string>` | `@blocking` |

### `get`

HTTPS GET. Returns the response or an error string on transport failure.

**Parameters:**

- `url`: `string`

**Returns:** `result<http-response, string>`

### `post`

HTTPS POST with a body and Content-Type header.

**Parameters:**

- `url`: `string`
- `body`: `string`
- `content-type`: `string`

**Returns:** `result<http-response, string>`

### `request`

General request — the curl/fetch-style escape hatch. `method` is any HTTP verb (GET/POST/PUT/PATCH/DELETE/HEAD); `headers` is a raw "Name: Value" block, one per line (LF-separated; empty for none); `body` is the request body (empty for none). Returns status + response headers + body.

**Parameters:**

- `method`: `string`
- `url`: `string`
- `headers`: `string`
- `body`: `string`

**Returns:** `result<http-response, string>`
