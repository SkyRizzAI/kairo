# 0008 — Hybrid raw radio access: three-axis model (Capability · Permission · Lease)

- **Status:** Accepted
- **Date:** 2026-06-23
- **Area:** core/hal, core/services, core/apps, platforms/esp32, platforms/wasm
- **Plan:** 87 — App Capability, Radio HAL & Permission

## Context

Palanu targets pentest/audit use cases (e.g. WiFi Marauder equivalent) where
third-party WASM apps need direct 802.11 radio access: passive scan, monitor
mode (promiscuous capture), raw frame injection, and thick attack loops
(deauth flood, beacon spam). This creates three hard problems:

**P1 — Access control.** Radio is a sensitive resource. An app silently
deauthing a network at boot, or continuously promiscuously capturing frames,
is a security and privacy violation. But requiring a permission prompt for
every scan would be unusable.

**P2 — Exclusive ownership.** The physical WiFi radio has one hardware
context. "Connected to AP" and "monitor mode" are mutually exclusive. Two
apps cannot hold the radio simultaneously. The system's managed connection
must yield when an audit app takes over, and restore when the app exits.

**P3 — Crash safety.** A WASM app that hangs or crashes while holding the
radio would strand the hardware permanently. The device would never reconnect
to Wi-Fi and the resource would be stuck until reboot.

Several approaches were considered:

1. **Deny everything** — third-party apps cannot touch the radio. Simple, but
   eliminates Palanu's key differentiator over Flipper (native radio vs.
   UART-connected ESP32).

2. **Allow everything** — apps call radio functions freely. Fails P1 (no
   consent) and P3 (no cleanup on crash).

3. **Per-app capability flags only** — manifest declares `net.wifi.monitor`,
   granted at install time. Still fails P2 (no mutual exclusion) and P3.

4. **Three-axis model (chosen):** each access requires all three axes to be
   satisfied simultaneously.

## Decision

App radio access is guarded by three independent gates that must all be
satisfied for a call to succeed:

```
  Capability   — board has this hardware feature (static, from IBoard)
  Permission   — user has explicitly granted this app (persistent, tiered)
  Lease        — app holds the exclusive lock right now (runtime, revocable)
```

### Axis 1 — Capability

`CapabilityRegistry` lists hardware features the board was built with.
Apps and the generated gating prologues check `rt.capabilities().has("net.wifi.monitor")`
before touching any WiFi surface. A board without WiFi hardware cannot grant
the capability regardless of permission.

### Axis 2 — Permission

`PermissionService` persists per-app, per-capability grants in device config.
The tier model:

| Tier | Examples | First-use behaviour |
|---|---|---|
| `benign` | `net.wifi.scan`, `storage.read` | Auto-grant, no prompt |
| `sensitive` | `net.wifi.monitor`, `net.wifi.inject` | Prompt on first use |
| `dangerous` | reserved | Prompt + confirmation text |

`PermissionScreen` is pushed by `PermissionService::factory` at the point of
first use — not at install time. This is intentional: the user sees the
request in context (app is running, user knows why) rather than at a
cold install prompt they will dismiss reflexively.

`PermissionService::revoke()` (added Fase 7) writes `denied=2` to the same
config key. Re-grant happens organically on next use (another prompt).
Settings → Apps → [app] shows and toggles per-cap status.

### Axis 3 — Lease

`ResourceBroker` maintains exclusive ownership of named resources. The key
resources are grouped by `ExclusivityGroup`:

```
"system:wifi" group:
    net.wifi.managed   — managed STA connection (system holds by default)
    net.wifi.monitor   — promiscuous capture (app must acquire)
    net.wifi.inject    — frame injection / thick attack loops (app must acquire)
```

Rules:
- Only one member of a group can be held at a time.
- The group has a `yieldableOwner` ("system:wifi") — the system's managed
  connection can be pre-empted by apps that acquire `monitor` or `inject`.
  The system is notified via `ResourceSuspended` event, disconnects WiFi,
  and re-acquires when `ResourceRestored` fires after the app releases.
- Non-yieldable system leases (e.g. `system:ota` during firmware download)
  block app acquisition — apps receive `LeaseError{busy, "system:ota"}`.
- Leases are scoped to the `AppHost` lifetime. `AppHostExited` (emitted
  unconditionally, even on crash) triggers `ResourceBroker::releaseAll(owner)`
  — this is the primary crash-safety mechanism.

`SystemWifiManager` (Fase 3) listens on `ResourceSuspended` / `ResourceRestored`
to drive `IWifiDriver::disconnect()` / `autoConnect()`.

### Hybrid thick/raw split (Axis 3 extension)

`IRadioWifi` exposes two access tiers to apps:

**Thick primitives** — app starts a named operation (deauth, beacon spam)
via one call; the firmware runs the timing-critical loop natively on ESP32
Core 0, never entering the WASM sandbox. The app receives matured events
(tick count, packet count) via `waitEvent()`. This avoids wasm3 JIT latency
in hard-realtime 802.11 TX contexts.

**Raw escape** — monitor mode gives the app a 128-slot bounded ring of raw
802.11 frames via `monitorRead()`. `inject()` pushes one frame directly.
The ring drops frames when full; the radio callback never blocks. Apps that
process frames too slowly lose frames gracefully rather than stalling the radio.

Both tiers require `net.wifi.inject` or `net.wifi.monitor` lease (per PIDL
`@lease` annotation on each function).

### WASM crash safety

A second crash-safety layer targets the specific failure mode of an infinite
loop inside the WASM sandbox (which `AppHostExited` alone cannot fix, because
the app thread never exits to emit it):

- `m3_Yield()` (wasm3 weak symbol, overridden with a strong definition)
  is called at every WASM function-call boundary. Our override checks a global
  `std::atomic<bool> g_vmAbort`; if set, returns `m3Err_trapAbort`.
- `AppHost::forceQuit()` sets `g_vmAbort` via `WasmEngine::requestAbort()`,
  which causes the WASM VM to exit at the next yield point and return exit
  code 130 (SIGKILL convention). The host then emits `AppHostExited`, which
  releases all leases.
- `WasmEngine::init()` enforces `rt_->memoryLimit` (passed from manifest
  `mem_quota_bytes`) via wasm3's `ResizeMemory` gate — a runaway app cannot
  allocate unbounded linear memory.

Known limitation: a tight loop that never makes a function call will not
hit `m3_Yield()` and will not be interrupted by `forceQuit()`. This is an
inherent limitation of the wasm3 interpreter; a future migration to a JIT
with signal-based safepoints would address it.

### Generated gating prologues

The IDL code generator (`packages/idl/`) reads `@capability`, `@tier`, and
`@lease` annotations on `wifi.pidl` function declarations and inserts gating
prologues into the generated QuickJS bindings and WASM host imports. This
ensures the access-control rules are automatically enforced for every
function in every runtime — no per-function manual guards needed.

## Consequences

**Positive:**
- Apps get real, native radio access without UART intermediary — latency is
  sub-millisecond vs. ~10ms UART round-trip of the Flipper Zero architecture.
- The three-axis model is composable: adding new sensitive capabilities
  (e.g. BLE inject, Sub-GHz) only requires adding PIDL annotations and a
  new ExclusivityGroup entry — no new architectural work.
- System stability is guaranteed by two independent cleanup paths
  (`AppHostExited` for clean exits/soft crashes; `forceQuit()` for hung VMs).
- Permission revoke (Settings → Apps → [app]) gives users visible control
  without requiring app reinstall.

**Negative / Tradeoffs:**
- Three-axis complexity: a developer trying to debug "why did my call fail?"
  must check all three axes. Error messages from the gating prologues identify
  which axis failed (`no_capability`, `permission_denied`, `lease_busy`).
- Single-app model assumption: `g_vmAbort` is a global. Palanu currently runs
  one WASM app at a time (single `AppHost`), so this is safe. If concurrent
  WASM apps are ever added, the abort flag must become per-runtime.
- `m3_Yield()` override is a link-time symbol replacement (wasm3 declares it
  `M3_WEAK`). This is fragile if wasm3 is updated to make the symbol strong,
  or if a future WASM runtime does not have an equivalent hook.
- Beacon spam and deauth loops run on Core 0 with no rate-limiting by the
  firmware. The app can instruct the radio to flood continuously. This is
  appropriate for an authorized audit tool but must be reflected in the app
  store review policy.
