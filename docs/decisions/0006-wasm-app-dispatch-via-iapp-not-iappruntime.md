# 0006 — WASM apps dispatch via IApp (WasmApp:ComponentApp), not IAppRuntime

- **Status:** Accepted
- **Date:** 2026-06-22
- **Area:** core/apps, core/wasm

## Context

Plan 56/57 introduced `IAppRuntime` with three adapters: `JsRuntime`, `CBuiltinRuntime`, and
`WasmRuntime`. The intent was to dispatch all app types through a tier-based adapter. However,
during Plan 84 audit we discovered that none of the `IAppRuntime` adapters are ever called. The
actual dispatch paths for C++ and JS apps go directly through `IApp::run()` / `IApp::runProcess()`:
- C++ apps: `ComponentApp::run()` is called by `AppHostManager`
- JS apps: `JsApp::run()` drives `JsEngine` directly
- `WasmRuntime` existed but its `run()` was a no-op; the hardcoded 1024-byte wasm slice was never
  triggered from any real launch path

`IAppRuntime` is therefore **dead code**. Both `AppHostManager` and `AppRegistry::launchProcess()`
dispatch via virtual calls on `IApp*` — the `IAppRuntime` layer is bypassed completely.

## Decision

WASM apps are implemented as `WasmApp : ComponentApp`, mirroring `JsApp : ComponentApp`. `WasmApp`
owns its raw `.wasm` bytes (`std::vector<uint8_t> wasm_`) and implements `runProcess(ProcessContext&)`
directly — creates a `WasmEngine`, links WASI + `nema.*` imports, calls `_start`, propagates
`proc_exit()` exit code.

`WasmAppStore` mirrors `JsAppStore`: uses `std::list<std::unique_ptr<WasmApp>>` for stable
pointers, calls `rt.apps().installCustom(app, manifest)` to register with `AppRegistry`.

`papp_installer.cpp` reads the `runtime` field from `manifest.json` and routes:
- `"js"` → `JsAppStore::installApp()`
- `"wasm"` → `WasmAppStore::installApp()`

`IAppRuntime` and its adapters are left in place (not deleted) but remain unused. They represent an
alternative future design that was not carried forward.

## Consequences

- **WasmApp is load-bearing for WASM module lifecycle**: `WasmApp` owns the `std::vector<uint8_t>
  wasm_` containing the raw `.wasm` bytes. This eliminates the prior hardcoded 1024-byte slice
  (`engine.load(..., 1024)` in `wasm_runtime.cpp`) — the actual byte count is always passed.
- **Storage namespace comes from the manifest id, not argv**: `WasmEngine::runStart(ctx, appId)`
  accepts the bundle id from `WasmApp` (set from `manifest.json`). The `WasmHostCtx::appId` field
  is what `wasm_nema.cpp` uses to namespace `AppStorage` — it cannot be overridden by a CLI
  invocation's `argv[0]`.
- **Import link order is load-bearing**: `linkWasiImports()` and `linkNemaImports()` MUST be called
  before `m3_FindFunction()`. `m3_FindFunction` triggers lazy compilation of the module, which
  resolves import references at that point. Unlinked imports at that moment produce "missing import"
  errors. This is not enforced by the API — it's an ordering invariant the caller must uphold.
- **`m3_FreeModule` must not be called after `m3_LoadModule` succeeds**: the runtime owns the
  module; a second `m3_FreeModule` in the destructor causes a double-free. `WasmEngine::~WasmEngine`
  sets `mod_ = nullptr` without freeing; `m3_FreeRuntime` frees the module as part of runtime
  teardown.
- If the `IAppRuntime` layer is ever revived, the dispatch in `AppHostManager` and
  `AppRegistry::launchProcess()` must be updated to route through it — those sites currently call
  `IApp*` directly.
