# Custom Apps (JS) & SDK

> Plans 37 / 49 / 58 / 59 — Write device apps in TSX, build them with the SDK, and install them onto
> the device — over the wire (volatile) or onto the filesystem — running on the embedded QuickJS
> engine through the same native UI components as built-in apps.

## Overview

A developer writes an `App.tsx` using `import { View, Text, Pressable, useState } from "nema"`. The
SDK bundles it; it runs on the device's QuickJS engine and renders into the native `UiNode` tree
(flex layout, focus ring, scrolling — all free). Apps are sandboxed (memory/stack/deadline limits)
and capability-gated.

## Packaging — `.papp`

| Shape | Layout | Install path |
|---|---|---|
| **`.papp` folder** | `manifest.json` + `app.js` (build output) | copy to `/apps/` on the device filesystem (hot-scanned by `papp_installer`) |
| **`PAPP1` container** | single-file `PAPP1\n<manifest>\n<entry>\n<js>` or a TOC bundle (`papp_package.h`) | OTA over the wire (`RemoteSession.installApp()` → PLP Ext `AppInstall`), and firmware embedding |

Manifest (`PappManifest`): `id, name, version, entry, needs[]` (capabilities), `api_version`
("major.minor" — checked at load: major exact, app.minor ≤ host.minor), plus `runtime`
(`js`|`wasm`|`native`), `display_server`, `icon`, `category`.

## How it works

```
Author App.tsx ──(palanu-build)──▶ dist/<id>.papp folder (manifest.json + app.js)
   PAPP1 wire ── PLP Ext AppInstall ──▶ JsAppStore.installPappBytes() ──▶ AppRegistry (Custom, RAM-only)
   .papp folder ── copy to /apps ────▶ papp_installer scan ──▶ launched from AppList / CLI PATH
On device: JsEngine (per-app JSRuntime on the app thread) → render() → native UiNode tree
           capability-gated `nema` global (generated bindings) ; `process` global (argv/exit/stdout)
```

- **Sandboxing**: per-app memory limit, max stack, deadline interrupt; QuickJS overflow guard set
  below the real thread stack so runaway scripts throw a catchable error.
- **RAM-only install** for apps pushed over the wire — appears immediately, lost on reboot;
  persistent install needs a flash FS (a `.papp` folder under `/apps`).
- **System API is generated from the IDL** — the firmware bindings and the SDK's `nema.d.ts` come
  from one source (`api/*.pidl`). See [`../architecture/scripting-and-apps.md`](../architecture/scripting-and-apps.md).

## File reference

| Area | File |
|---|---|
| Engine | `firmware/core/src/js/{js_engine,js_api,nema_host_impl}.cpp`, `include/nema/js/nema_runtime_js.h` |
| App / store | `firmware/core/include/nema/apps/{js_app,js_app_store}.h`, `embedded_apps.h` (generated built-ins) |
| SDK | `packages/app-sdk/bin/build.ts` (`palanu-build`), `src/{manifest,index,system}.ts`, `templates/*` |
| SDK codegen | `packages/app-sdk/scripts/gen-runtime-header.ts` |
| Installer | `firmware/core/src/app/papp_installer.cpp` (scan `/apps` + `/sd/apps`, hot-reload) |
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
- Headless WASM apps run via wasm3 (tier `wasm`) — see the architecture doc.
