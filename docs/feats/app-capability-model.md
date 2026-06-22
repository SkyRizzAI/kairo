# App Capability Model

> Implemented by Plan 87. Builds on Plan 86 (process-first app model) and Plan 83
> (app storage). The three axes — **Capability**, **Permission**, **Lease** — control
> what hardware a WASM/JS app can access and what happens when it crashes or exits.

---

## Three-axis access control

| Axis | Question | Service | Persistence |
|---|---|---|---|
| **Capability** | Does the board have this hardware? | `CapabilityRegistry` | build-time |
| **Permission** | Has the user granted this to the app? | `PermissionService` | NVS |
| **Lease** | Does the app hold the exclusive resource now? | `ResourceBroker` | runtime only |

All three must pass before a sensitive host function executes. The generated
host-ABI prologue (from `api/*.pidl`) checks them in order and returns an error
if any gate fails — the WASM call returns an error, the app is not terminated.

---

## Capability tiers

```
@tier(benign)     — no permission or lease required (e.g. scan)
@tier(sensitive)  — permission + exclusive lease required (e.g. deauth, inject)
```

Tiers are declared in the PIDL source (`firmware/api/*.pidl`). The code generator
emits the gate prologue for each function automatically.

---

## ExclusivityGroup — radio bus conflict

Three capabilities share the same physical WiFi radio:

| Capability | Who holds it | Can it yield? |
|---|---|---|
| `net.wifi.managed` | `system:wifi` (STA connection) | yes |
| `net.wifi.monitor` | app | no |
| `net.wifi.inject` | app | no |

`ResourceBroker::addExclusivityGroup("wifi.radio", [...], "system:wifi")` registers
these as an exclusivity group. When an app acquires `monitor` or `inject`, the broker:
1. Emits `ResourceSuspended` → `SystemWifiManager` calls `wifi->disconnect()`.
2. Grants the app's lease.

When the app releases or exits, the broker emits `ResourceRestored` →
`SystemWifiManager` reacquires `managed` and calls `wifi->autoConnect()`.

---

## Lease lifetime and crash safety

`ResourceBroker` tracks every lease by `(appId, handle)`. When `AppHostExited` is
posted (always, regardless of exit code — see `app_host.cpp:threadEntry()`),
`releaseAll(appId)` is called. This covers:

- Normal exit (`proc_exit`)
- WASM trap (`m3Err_trapExit` or other)
- Watchdog-killed apps (`m3Err_trapAbort` → exit code 130)
- JS exceptions
- Native C++ exceptions caught by `AppHost::threadEntry`

**I5 — Offensive = sandboxed.** Apps that access radio hardware MUST be WASM or JS.
Native C++ code is first-party and trusted; it bypasses the lease gate. Any
first-party code calling `esp_wifi_80211_tx` directly without holding a lease is
a bug, not a security boundary.

**I6 — Lease = runtime only.** Leases are never persisted. A device restart always
starts clean. Apps re-acquire leases on each launch; the permission is persisted but
the lease is not.

---

## Watchdog — force-quitting hung apps (Fase 6)

wasm3's `m3_Yield()` is a weak symbol called at every WASM function-call boundary.
Palanu overrides it with a strong definition that checks a global atomic flag
(`g_vmAbort`). When the flag is set, the interpreter traps with `m3Err_trapAbort`
and the engine exits immediately.

API surface:

```cpp
// From any thread — idempotent:
WasmEngine::requestAbort();   // sets g_vmAbort
app.requestAbort();           // calls WasmEngine::requestAbort() for WasmApp
host.forceQuit();             // requestStop() + requestAbort() + unpause
```

`forceQuit()` on `AppHost` combines cooperative (`requestStop`) + escalated
(`requestAbort`) exit. The thread is not forcefully killed — the next WASM function
call traps and `runStart()` returns with exit code 130. `AppHostExited` fires
normally, leases are released, and the system restores STA WiFi.

**Known limitation:** a WASM `while(1){}` with no function calls (branch-only tight
loop) does not trigger `m3_Yield()`. On ESP32, the FreeRTOS hardware watchdog will
reset the device if the task starves the idle tick. On WASM, the browser tab
supervisor handles it. A future Fase could add a timer that polls `thread_.running()`
and calls `vTaskDelete()` as a last resort on hardware.

---

## Monitor ring buffer backpressure

`IRadioWifi` has two separate bounded queues:

| Queue | Slots | Consumer | Producer |
|---|---|---|---|
| `eventQ_` | 64 | `waitEvent()` (app) | `pushEvent()` (native loop) |
| `monitorQ_` | 128 | `monitorRead()` (app) | `pushFrame()` (RX callback) |

Both are `MessageQueue<vector<uint8_t>>` — FIFO, mutex-backed. `send()` drops the
item if the queue is full; the radio callback never blocks. A slow app loses frames
and must recover by reading faster or accepting the data loss. The kernel and the
radio remain responsive regardless of app throughput.
