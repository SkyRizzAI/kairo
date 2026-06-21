# Scripting Runtimes & App Packaging

> Two embedded interpreter tiers run sandboxed third-party apps: **QuickJS** for UI apps
> (the primary path) and **wasm3** for headless processes. Both sit behind `IAppRuntime`,
> selected by the app manifest's runtime tier.

## QuickJS — UI apps

`firmware/core/src/js/`, headers `nema/js/`.

- **`JsEngine`** (`js_engine.*`): one `JSRuntime`+`JSContext` per app, on the app thread (so JS
  GC never touches the UI thread). `loadApp()` evaluates a `.papp` bundle as an **ES module**,
  resolving `import … from "nema"` to an **embedded runtime** (`nema_runtime_js.h` — a generated
  minified `NEMA_RUNTIME_JS` string providing jsx/View/Text/Pressable/ScrollView/Slider/
  useState/useRef/useEffect/renderToTree). `render(arena)` reifies the JS component's default
  export into a native `ui::UiNode` tree each frame; `callHandler(id)` fires onPress/onChange and
  reports re-render via `takeDirty()`.
- **Sandboxing**: `setMemoryLimit`, `setMaxStackSize`, `setDeadlineMs` + an interrupt trampoline.
  `JsApp::stackBytes()` gives the app thread a generous stack (256 KB on ESP in PSRAM, 512 KB on
  host) with QuickJS's overflow guard set strictly below it, so a runaway/too-deep script throws a
  clean **catchable** error instead of corrupting the real thread stack.
- **API bindings (Plan 49)**: `setHost(rt, appId)` installs a capability-gated `nema` global. The
  bindings are **generated** (`generated/host/nema_api_quickjs.gen.cpp`, `installNemaApi`) backed
  by a hand-written `HostApi`/`NemaHostImpl` (`nema_host_impl.cpp`); `js_api.cpp` wires them and
  adds flat-path shims (`nema.log`, `nema.storage.get/set`, …). The host impl **gates on
  capabilities** (`http_get` returns "http not available" with no `IHttpClient`; wifi methods
  stubbed). `setProcessContext` installs a `process` global (`argv`, `exit(n)` throws to unwind,
  `stdout.write`).
- **`JsApp`** (`apps/js_app.h`) is a `ComponentApp` (gets flex layout/focus/scroll/gestures for
  free). **`JsAppStore`** (`apps/js_app_store.h`) is the process-wide owner: `installApp`/
  `installPappBytes` register apps live into the `AppRegistry` as `AppKind::Custom` — **RAM-only
  and volatile** (Palanu's portable-JS analog of Flipper's FAP loader). `installPappBytes` parses
  the single-file container `PAPP1\n<manifest>\n<entry>\n<js>`.
  **`JsRuntime`** (`js/js_runtime.h`) is the `IAppRuntime` adapter (tier "js").

## wasm3 — headless processes

`firmware/core/src/wasm/`, headers `nema/wasm/`.

- **`WasmEngine`** (`wasm_engine.*`): a thin wrapper over the wasm3 interpreter, one per app.
  Lifecycle `init(stackBytes, memQuota=65536)` → `load(wasm,len)` → `runStart(ctx)` (calls
  `_start`, returns exit code). wasm3's built-in WASI is **disabled** — Palanu owns the mapping.
- **WASI shim** (`wasm_wasi.cpp`, `linkWasiImports`): a minimal `wasi_snapshot_preview1` surface
  bound to `ProcessContext` — `args_sizes_get/args_get`, `fd_write` (fd2→stderr else stdout),
  `fd_read` (fd0 only), `proc_exit`. **Every guest pointer is bounds-checked** against
  `m3_GetMemorySize` before access — that is the sandbox boundary. There is no filesystem/network
  access from a WASM process.
- **`WasmRuntime`** (`wasm/wasm_runtime.h`) is the `IAppRuntime` adapter (tier "wasm"):
  `runProcess` loads `.wasm` → engine → WASI → `_start`. `runUi` is a no-op (deferred).

## App packaging — `.papp`

Authored with the **app-sdk** (`packages/app-sdk/`) in TSX (React/Ink-style) running on the
device's QuickJS, rendering through the **same native components** as built-in apps.

- **Author**: `App.tsx` using `import { View, Text, Pressable, useState } from "nema"` + a
  manifest (`PappManifest`, `src/manifest.ts`): `id, name, version, entry, needs[]` (capabilities),
  `api_version` ("major.minor" — checked at load: major exact, app.minor ≤ host.minor), plus Plan 59
  fields `runtime` (`js`|`wasm`|`native`), `display_server`, `icon`, `category`.
- **Build** (`bin/build.ts`, `palanu-build`): bundles the entry via `Bun.build` (ESM, minified,
  `external: ["nema", …]` so app + host share one runtime). Output is a **`.papp` folder**
  (`dist/<id>.papp/` with `manifest.json` + `app.js`); install by copying it to `/apps/` on the
  device (filesystem install, hot-scanned by `papp_installer`).
- **Wire / single-file container**: the OTA path (`RemoteSession.installApp()` → `AppInstall`
  channel → `JsAppStore::installPappBytes`) and the `parsePapp` loader accept the **`PAPP1`**
  container (single-file `PAPP1\n<manifest>\n<entry>\n<js>` or a TOC-concatenated bundle with
  assets — see `papp_package.h`).
- **Embedding into firmware**: `firmware/core/include/nema/apps/embedded_apps.h` holds the built-in
  "Embedded" app store (currently Sys Info + Counter) as extracted JS strings, installed at boot via
  `loadEmbeddedJsApps` → `installApp`. `scripts/gen-runtime-header.ts` bundles the SDK runtime into
  `firmware/core/include/nema/js/nema_runtime_js.h` so the device resolves `nema` against the same
  runtime as built-in apps.
- **Templates**: `counter`, `sysinfo`, `hello-papp`.

## The IDL — single source of truth for the System API

`packages/idl/` (Plans 48/49). Interfaces/records/funcs are described in a WIT-subset `.pidl`
language (sources in `api/*.pidl`: sys, input, media, aether-ui, bt, profile, net, storage).

- **Parser** (`src/parser.ts`): `.pidl` → `api/build/nema-api.json` AST (`--check` for CI).
- **Generator** (`src/gen.ts`, idempotent `writeIfChanged`) emits from one definition:
  - docs `docs/api/*.md` + `parity.md`,
  - firmware host bindings `generated/host/nema_api.gen.h` + `nema_api_quickjs.gen.cpp`,
  - SDK `generated/sdk/nema.d.ts` (replaces the hand-written `system.ts`) + `nema.h` (WASM C header).

So the IDL feeds **both** the firmware's JS API bindings and the SDK's TypeScript types.
`@blocking` funcs become `Promise<>`; capability annotations become gate notes.

## Conventions & gotchas

- **JS apps install RAM-only and volatile** — pushed over the wire they appear immediately but are
  lost on reboot; persistent install needs a flash FS (no SD required).
- **QuickJS overflow guard is deliberately below the real thread stack** so deep recursion throws
  instead of crashing.
- **wasm3 WASI is intentionally disabled**; a guest gets only the five bounds-checked syscalls in
  `wasm_wasi.cpp`.
- **Never hand-edit `generated/`** — the IDL is SSOT; `gen.ts --check` / `parser.ts --check` are CI
  gates. Files marked `// @generated … DO NOT EDIT` (`embedded_apps.h`, `nema_runtime_js.h`,
  everything under `generated/`) are machine-written.
- **Regenerate on change**: `gen-runtime-header.ts` when SDK components/hooks change;
  `idl gen.ts` when `.pidl` changes.
- **One packaging format** — `.papp`: a folder (`manifest.json` + `app.js`) for filesystem install,
  or the `PAPP1` container (single-file / TOC bundle) for the OTA wire. `api_version` is the runtime
  compatibility contract.

## Key files

| Area | File |
|---|---|
| QuickJS engine / bindings | `firmware/core/include/nema/js/js_engine.h` + `src/js/{js_engine,js_api,nema_host_impl}.cpp`, `include/nema/js/nema_runtime_js.h` |
| JS app / store / adapter | `firmware/core/include/nema/apps/{js_app,js_app_store}.h`, `include/nema/js/js_runtime.h` |
| Embedded apps (generated) | `firmware/core/include/nema/apps/embedded_apps.h` |
| wasm3 engine / WASI / adapter | `firmware/core/include/nema/wasm/{wasm_engine,wasm_runtime}.h` + `src/wasm/{wasm_engine,wasm_wasi}.cpp` |
| app-sdk | `packages/app-sdk/bin/build.ts`, `src/{manifest,system,index}.ts`, `scripts/gen-runtime-header.ts`, `templates/*` |
| IDL | `packages/idl/src/{parser,gen,ast}.ts`, `src/emit/*.ts`, `api/*.pidl` |
