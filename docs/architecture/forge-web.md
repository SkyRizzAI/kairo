# Palanu Forge (Web Host)

> The host-side companion: a SvelteKit app that runs the firmware **simulator** in the
> browser (WASM), connects to **real hardware** over Web Bluetooth / Web Serial, **flashes**
> firmware client-side, and **installs** custom apps — all over the same PLP protocol.

## Purpose

**Palanu Forge** (`packages/forge/`) is a SvelteKit (Svelte 5 runes) app, essentially client-only
(`ssr = false`). The server exists only to serve the WASM blob with the right headers and expose a
firmware registry. Unifying idea: **one protocol (PLP), many transports** — BLE, USB-Serial, and
the WASM virtual cable are interchangeable behind one `ILinkTransport`, so `RemoteSession` and the
whole UI are transport-agnostic.

## Browser transports

All implement `ILinkTransport` (`src/lib/plp/transport.ts`): `kind`, optional `boot()`,
`send(bytes)`, `onData(fn)`, `onState(fn)`, `isConnected()`, `close()`. (Also `loopbackPair()` for
tests — the virtual-cable model.)

- **SerialTransport** (`transport/SerialTransport.ts`, `kind='usb'`) — Web Serial / USB-CDC.
  `connect(baudRate = 921600)` → `navigator.serial.requestPort()` → `port.open(...)`. PLP frames
  are self-delimiting so the `FrameParser` reframes the stream. `close()` ordering is fragile
  (cancel reader → await loop → release writer lock → close port); skipping a step leaves the OS
  port locked. (The **flasher** drives DTR/RTS reset via esptool-js; this data path does not.)
- **BleTransport** (`transport/BleTransport.ts`, `kind='ble'`) — Web Bluetooth GATT.
  `requestDevice({filters:[{services:[PLP_SERVICE]}]})`, subscribe **TX** notifications
  (device→host), write **RX** (`writeValueWithoutResponse`). Writes must be ArrayBuffer-backed
  copies (GATT rejects SharedArrayBuffer views on the isolated page). UUIDs in `plp/uuids.ts`:
  service `a7b30001-…`, TX `a7b30002-…`, RX `a7b30003-…`.
- **VirtualCableTransport** (`transport/VirtualCableTransport.ts`, `kind='wasm-usb'`) — the
  in-process WASM simulator cable. `boot()` lazy-loads the **classic (non-ESM) Emscripten** build
  via a `<script>` tag (ESM transform mangles `import.meta.url` and breaks pthread workers),
  pre-defining `window.Module` with `locateFile → /fw/`, an `nemaPlpOut` callback, and
  `onRuntimeInitialized`; loads `/fw/palanu.js`. `send()` mallocs into `HEAPU8` and calls
  `_nema_plp_recv`; inbound arrives via `Module.nemaPlpOut`.

**Cross-origin isolation (load-bearing).** Emscripten pthreads need SharedArrayBuffer → the page
must be cross-origin isolated (COOP `same-origin` + COEP `require-corp`), and the WASM worker
scripts must carry CORP. Headers are set in three places kept in sync: `src/hooks.server.ts`
(prod), `vite.config.ts` (dev/preview), and `src/routes/fw/[file]/+server.ts` (serves only
`palanu.js`/`palanu.wasm` with COEP + **CORP `same-origin`** + COOP). Static handlers don't add
CORP, so the WASM is routed through `/fw/[file]` to guarantee headers — otherwise the worker hangs
at `onRuntimeInitialized`.

## PLP in TypeScript

`src/lib/plp/codec.ts` is the canonical TS codec; the firmware codec mirrors it byte-for-byte and
they share test vectors. Frame `[magic:0xAB][chan][flags][len:2 LE][payload][crc8]`, CRC-8/SMBus
(poly 0x07), 10 `Channel` values (Control…File), `Flags` (FragMore/Compressed), RLE for the 1-bit
framebuffer. `FrameParser` buffers, scans for `0xAB`, validates CRC, drops a byte and resyncs on
mismatch (handles split chunks, multiple frames per chunk, and log-noise interleaving).

**RemoteSession** (`src/lib/RemoteSession.ts`) — transport-agnostic, multi-listener PLP client.
Handshake: on transport connect it sends Control **HELLO** (`payload[0]=0x01`) and **retries every
300 ms** until ready; on **ACK** (`0x02`) it stops the timer and immediately sends **System
GetInfo** (`0x01`) for the board profile. Decodes Screen / Log / Event / Cli / File / Ota. APIs:
`sendKey`, `sendCli(sid,line)`, FILE ops (FIFO-correlated, 5 s timeout), `power(op)`,
`injectEvent`, `wifiSetNetworks`, `installApp(kapp)`, and `otaUpdate()` (Begin→Data×N→End, 1792 B
chunks, offset-idempotent retries, `OTA_PROTO=2`).

## UI / routes

Layout (`src/routes/+layout.svelte`) is a sidebar shell; `/` redirects to `/simulator`.

- **`/simulator`** — the WASM device, bound to the reactive `simStore`. Device starts **off**;
  **Boot** loads+boots WASM in place; **Restart** reloads the page with an `nema:autoboot`
  sessionStorage flag (WASM can load only once per page lifetime); **Shutdown** reloads to off.
  `BoardVisual`, CLI terminal, and toggleable panels (Settings / Files / Firmware OTA dry-run /
  Logs / Events / Services).
- **`/remote`** — discovery picker for Simulator / BLE / USB; builds a `RemoteSession` and renders
  the shared `SessionView`. Uses `remoteLink` for persistence.
- **`/install`** — upload/paste a `.kapp` (must start with `KAPP1`); pushes it to the running sim
  via the PLP Ext path (`installApp()`). Volatile, appears live.
- **`/flash`** — Web Serial firmware flasher via **esptool-js** (921600), fully client-side; loads
  the build list from tRPC `firmware.list`; writes bootloader `0x0` / partition `0x8000` / app
  `0x10000`. Also a standalone serial console (115200).
- **`/fw/[file]`** — the headered WASM endpoint (above).
- **`/api/trpc/[...]`** — tRPC: `firmware.list`/`firmware.version` from `static/firmware/
  manifest.json` (produced by `firmware/tools/publish-firmware.sh`). **`/api/firmware-proxy`** —
  GitHub-only allow-listed proxy so OTA `.bin` downloads bypass the COEP/CORP fetch restriction.

**Shared components**: `SessionView.svelte` is the single unified device-control UI used by
`/remote` (all transports) and mirrored by `/simulator`; `BoardVisual.svelte` renders the device
from its `BoardProfile` (bezel + components at normalized 0–1 coords, 1-bit framebuffer to a
pixelated canvas, with a generic D-pad fallback before the profile arrives). Plus `CliTerminal`,
`FileBrowser`, `FirmwarePanel`, and a shadcn-svelte `ui/` kit.

**State**: `remoteLink.ts` is a module-level singleton holding the active `{session, label,
owned}` so it survives SvelteKit navigation (without it, leaving `/remote` orphaned the session and
locked the OS serial port). The shared WASM session (`wasmSim.ts` `wasmSession()`) is site-wide and
only detached, never closed. `simStore.svelte.ts` is a runes-reactive wrapper over the WASM
session (`frame/profile/logs/events/services/connected/power` as `$state`; logs/events ring-buffered
to 300).

## Cross-repo contracts

See the [architecture README](README.md#cross-repo-contracts-must-stay-byte-for-byte-in-sync) for
the full table. The host↔firmware contracts that live in this package: the PLP codec
(`plp/codec.ts`), BLE UUIDs (`plp/uuids.ts`), the board-profile JSON shape (`RemoteSession.ts` /
`BoardVisual.svelte`), and the `.kapp` install format. The Nema System API types (`nema.d.ts`) and
the firmware host bindings are both generated from the IDL — see
[`scripting-and-apps.md`](scripting-and-apps.md).

## Conventions & gotchas

- **Forge is client-only** (`ssr=false`); transports need `available()` guards (Chromium-family
  only) and SSR-safe `typeof navigator` checks.
- **Naming transition**: UUIDs/codec comments and `wasmSim` still say "Kairo/KLP/nema"; the WASM
  blob was renamed `nema.js → palanu.js`; `installApp` checks `KAPP1` while the SDK build also emits
  `.papp`. Package scopes are `@palanu/*`. Don't assume one canonical name.
- **WASM single-load** — the firmware loads once per page lifetime; Restart = full reload with an
  autoboot flag; real teardown needs a reload.
- **Cross-origin isolation is load-bearing** — serve WASM via `/fw/[file]` and proxy GitHub OTA via
  `/api/firmware-proxy`; keep `hooks.server.ts` and `vite.config.ts` headers in sync.
- **Web Serial cleanup ordering** is fragile; **GATT writes** must be ArrayBuffer copies (~512 B
  cap); **OTA chunk = 1792 B** (4096 broke USB-CDC RX ring), offset-idempotent.

## Key files

| Area | File |
|---|---|
| PLP codec / UUIDs / transport iface | `packages/forge/src/lib/plp/{codec,uuids,transport}.ts` |
| Transports | `packages/forge/src/lib/transport/{SerialTransport,BleTransport,VirtualCableTransport}.ts` |
| Session / state | `packages/forge/src/lib/{RemoteSession,wasmSim,remoteLink,simStore.svelte}.ts` |
| Components | `packages/forge/src/lib/components/{SessionView,BoardVisual,CliTerminal,FileBrowser,FirmwarePanel}.svelte` |
| Headers / WASM serving | `packages/forge/{src/hooks.server.ts,vite.config.ts}`, `src/routes/fw/[file]/+server.ts` |
| Routes | `packages/forge/src/routes/{simulator,remote,flash,install}/+page.svelte`, `api/{trpc,firmware-proxy}` |
| Firmware registry | `packages/forge/src/lib/trpc/router.ts`, `static/firmware/manifest.json` |
