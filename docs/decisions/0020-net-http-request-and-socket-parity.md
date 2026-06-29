# 0020 — Net API: a curl-style HTTP `request`, and why raw sockets are deferred

- Status: accepted
- Date: 2026-06-29

## Context

The JS/WASM app API (`nema:net`) exposed only `http.get` and a stubbed
`http.post` ("not yet implemented"). The first real consumer — the Web3 Test
app's Solana **devnet faucet** — needs a JSON-RPC `POST`, so `post` had to become
real. Beyond that, the ask was a "lebih low level" net service "kaya curl atau
nodejs": arbitrary methods, custom headers, request/response bodies, and ideally
raw TCP/UDP.

Two hard constraints shaped the design:

1. **1=1 parity is a product promise.** Every capability the firmware sells must
   be *equally* usable from JS custom apps and from the WASM simulator — not
   firmware-only. The UI framework already holds this line; the net API must too.
2. **The simulator runs in a browser sandbox.** A browser cannot open raw TCP or
   UDP sockets — only `fetch()` / WebSocket. So a raw-socket API *cannot* run on
   WASM without a relay server, which would break parity (or add infra).

The IDL generator (`packages/idl`) is the SSOT: `api/*.pidl` → `nema-api.json` →
host abstract (`generated/host/nema_api.gen.h`), QuickJS bindings, `.d.ts`, WASM
header. No function in the IDL has ever taken a *record* parameter — only
primitives/handles — so a `headers: list<header>` request object is unproven in
the emitter.

## Decision

**Add a general `http.request(method, url, headers, body) → {status, headers,
body}` and make `post` real; defer raw TCP/UDP sockets.**

- `request` is the curl/fetch-style escape hatch: any verb
  (GET/POST/PUT/PATCH/DELETE/HEAD), a **raw `"Name: Value\n"` header block**
  (string, not a record — stays within the proven IDL surface), and a body.
- `http-response` gains a `headers` string (raw response headers) alongside
  `status` and `body`. `get`/`post` now delegate to the same path.
- Device transport: `Esp32HttpClient` implements `request()` over
  `esp_http_client` (verb map, per-line `set_header`, post field, response
  headers collected via `HTTP_EVENT_ON_HEADER`). `get`/`post` are thin wrappers.
- **Raw TCP/UDP sockets are NOT added.** They are feasible on-device (lwIP) but
  impossible in the browser sandbox, so they would be firmware-only — a 1=1
  violation. We keep the API to what both runtimes can honour.

Header ergonomics (`{ Authorization: "…" }`) are left to an optional app-side
helper that serialises to the raw block; the wire/IDL surface stays simple.

## Consequences

- **Apps get a real HTTP client** (faucet works; any REST/RPC call is now
  possible) with identical shape on device and simulator — parity surface holds.
- **Functional parity has one open gap:** there is still **no WASM `IHttpClient`**,
  so on the simulator `http.*` returns `"http not available"` until a
  browser-`fetch` bridge is added. The *surface* is 1=1 today; the *transport*
  is device-only until that bridge lands. This is tracked as the next net task.
- Choosing a **string header block** over a `list<header>` record avoids
  exercising an untested generator path; the cost is slightly less typed
  ergonomics, recoverable via an app-side wrapper.
- Raw TCP/UDP remains available as a **future, capability-gated, device-only**
  option *iff* we accept breaking 1=1 for sockets (or add a WS relay) — a
  separate decision, not made here.
- Bumps no API major; `request` is additive and `http-response.headers` is a new
  field existing consumers ignore.
