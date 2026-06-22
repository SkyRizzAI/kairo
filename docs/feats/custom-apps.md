# Custom Apps (JS & WASM) & SDK

> Plans 37 / 49 / 58 / 59 / 84 — Write device apps in TSX (JS) or C (WASM), build them with the
> SDK, and install them onto the device — over the wire (volatile) or onto the filesystem. JS apps
> run on the embedded QuickJS engine; WASM apps run on wasm3 headless (UI deferred). All three
> runtimes (C++ built-in, JS, WASM) share the same manifest, launcher icon, CLI/UI/Hybrid dispatch,
> and `nema.*` API surface.

## Overview

A developer writes an `App.tsx` (JS) or `main.c` (WASM) and builds with the SDK. JS apps render into the native `UiNode` tree via QuickJS bindings; WASM apps run headless (CLI mode). Apps are sandboxed and capability-gated. All runtimes expose the same `nema.*` API.

### App modes (Plan 84)

| Mode | Launch behaviour | `manifest.json` |
|---|---|---|
| `ui` (default) | Opens UI canvas via `AppHostManager` | `"mode": "ui"` |
| `cli` | Runs headless on a new thread — `args→stdout→exitCode` | `"mode": "cli"` |
| `hybrid` | UI when launched from launcher; CLI when launched from shell | `"mode": "hybrid"` |

`AppRegistry::launch()` reads `AppManifest.mode` and routes to `AppHostManager` (UI) or `launchProcess()` (headless thread). WASM apps are forced to `cli` until WASM UI is implemented (Fase 4b).

## Packaging — `.papp`

| Shape | Layout | Install path |
|---|---|---|
| **`.papp` folder** | `manifest.json` + `app.js` (build output) | copy to `/apps/` on the device filesystem (hot-scanned by `papp_installer`) |
| **`PAPP1` container** | single-file `PAPP1\n<manifest>\n<entry>\n<js>` or a TOC bundle (`papp_package.h`) | OTA over the wire (`RemoteSession.installApp()` → PLP Ext `AppInstall`), and firmware embedding |

Manifest (`PappManifest`): `id, name, version, entry, needs[]` (capabilities), `api_version`
("major.minor" — checked at load: major exact, app.minor ≤ host.minor), plus `runtime`
(`js`|`wasm`|`native`), `display_server`, `icon`, `category`, `mode` (`ui`|`cli`|`hybrid`).

## How it works

```
JS app:
  Author App.tsx ──(palanu-build)──▶ dist/<id>.papp folder (manifest.json + app.js [+ icon.raw])
     PAPP1 wire ── PLP Ext AppInstall ──▶ JsAppStore.installPappBytes() ──▶ AppRegistry (Custom, RAM-only)
     .papp folder ── copy to /apps ────▶ papp_installer scan ──▶ launched from AppList / CLI PATH
  On device: JsEngine (per-app JSRuntime on the app thread) → render() → native UiNode tree
             capability-gated `nema` global (generated bindings); `process` global (argv/exit/stdout)

WASM app:
  Author main.c ──(wasi-sdk)──▶ main.wasm  ─┐
                                             ├──▶ dist/<id>.papp folder (manifest.json + main.wasm [+ icon.raw])
  manifest.json: "runtime": "wasm", "mode": "cli"
  On device: WasmEngine (wasm3) — links WASI + nema.* imports, runs _start, propagates proc_exit code
             nema.* bridge: log, device_name, device_caps, storage_fs_read_file, storage_fs_write_file
```

- **Sandboxing**: per-app memory limit, max stack, deadline interrupt; QuickJS overflow guard set
  below the real thread stack so runaway scripts throw a catchable error.
- **RAM-only install** for apps pushed over the wire — appears immediately, lost on reboot;
  persistent install needs a flash FS (a `.papp` folder under `/apps`).
- **System API is generated from the IDL** — the firmware bindings and the SDK's `nema.d.ts` come
  from one source (`api/*.pidl`). See [`../architecture/scripting-and-apps.md`](../architecture/scripting-and-apps.md).
- **WASM UI is not yet implemented** (Plan 84 Fase 4b — deferred): WASM apps that declare `mode: ui` are coerced to `cli` with a warning.

## Launcher icon pipeline (Plan 84 Fase 2)

Each `.papp` can include an `icon.raw` file — a 1-bit monochrome bitmap matching the system's
built-in icon format (`icon_pack`). The SDK converts any `icon.png` in the source directory
automatically during `palanu-build`. Format:

```
icon.raw = width(u16le) + height(u16le) + pixels(1-bit packed, MSB first, row-major)
stride   = ceil(width / 8) bytes per row
```

`AppListScreen` renders custom icons via the existing `Icon()` node (no new renderer needed).
If no `icon.raw` is present, the launcher falls back to the built-in `icon_pack` for that category.

## File reference

| Area | File |
|---|---|
| JS engine | `firmware/core/src/js/{js_engine,js_api,nema_host_impl}.cpp`, `include/nema/js/nema_runtime_js.h` |
| JS app / store | `firmware/core/include/nema/apps/{js_app,js_app_store}.h`, `embedded_apps.h` (generated built-ins) |
| WASM engine | `firmware/core/src/wasm/wasm_engine.cpp`, `wasm_wasi.cpp`, `wasm_nema.cpp` |
| WASM app / store | `firmware/core/include/nema/apps/{wasm_app,wasm_app_store}.h` |
| CLI dispatch | `firmware/core/src/app/app_registry.cpp` (`launchProcess()`), `process_context.h` |
| SDK | `packages/app-sdk/bin/build.ts` (`palanu-build`), `src/{manifest,index,system}.ts`, `templates/*` |
| SDK codegen | `packages/app-sdk/scripts/gen-runtime-header.ts` |
| Installer | `firmware/core/src/app/papp_installer.cpp` (scan `/apps` + `/sd/apps`, routes JS→`JsAppStore`, WASM→`WasmAppStore`) |
| IDL | `packages/idl/`, `api/*.pidl` |

## Usage

```bash
# Scaffold from a template (counter / sysinfo / hello-papp), then:
palanu-build           # → dist/<id>.papp/  (manifest.json + app.js)
```
Install: copy the `.papp` folder to `/apps/` on the device (or push it over the wire via
`RemoteSession.installApp()` → PLP Ext `AppInstall`). The app appears in the AppList; CLI can
auto-launch apps on the `PATH`.

## Extending / regeneration (don't hand-edit generated files)

- Change SDK components/hooks → re-run `gen-runtime-header.ts` (updates `nema_runtime_js.h`).
- Built-in apps live in `embedded_apps.h` (extracted JS strings, installed at boot via
  `loadEmbeddedJsApps`).
- Change the System API → edit `api/*.pidl`, re-run the IDL generator (updates firmware bindings +
  `nema.d.ts`). `--check` modes are CI gates.
- Add WASM host imports → extend `wasm_nema.cpp` (`linkNemaImports`) and matching SDK headers.
- WASM UI (Fase 4b, not yet built): IDL generator will emit WASM import table for aether ABI.
