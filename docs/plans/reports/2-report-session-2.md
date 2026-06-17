# Implementation Report: Plans 47–60 (Session 2)

Audit komprehensif codebase terhadap Plans 47–60. Mencakup semua yang sudah di-implement,
file-file baru/diubah, dan status tiap plan per state codebase saat ini.

---

## Status Ringkasan

| Plan | Judul | Status | Keterangan |
|------|-------|--------|------------|
| 47 | App Platform Overview | ✅ | Foundation — direalisasi lewat Plans 48–60 |
| 48 | Nema System API IDL | ✅ | IDL parser + codegen di `packages/idl/` |
| 49 | SDK Binding Generator | ✅ | Binding auto-generated ke `js_api.cpp` |
| 50 | UI SDK Model | ✅ | `node.h`, `widgets.h`, `layout.cpp`, `renderer.cpp` |
| 51 | Display Server Architecture | ✅ | `registerServer()` + `extraServers_` di GuiService |
| 52 | Aether UI SDK | ✅ | SmartLabel, TitleBar, ListItem, scroll, text roles |
| 53 | Theming & Asset Packs | ✅ | IconPack (16 XBM), style tokens, theme() |
| 54 | Process Model | ✅ | ProcessManager, Pipe, ProcessContext, CLI `run` |
| 55 | Surface & Window Model | ✅ | ISurface, IWindowPolicy, SingleForegroundPolicy |
| 56 | App Runtime Architecture | ✅ | AppHost, AppHostManager, IAppRuntime, BuiltinRuntime |
| 57 | WASM Runtime | ✅ | wasm3 vendor, WasmEngine, WasmRuntime, WASI stubs |
| 58 | JS Runtime Process Model | ✅ | JsEngine HostApi, process.exit, NemaHostImpl |
| 59 | App Manifest & Packaging | ✅ | .papp folder (macOS style), loadInstalledPapps, cache |
| 60 | Aether UI Rewrite | ✅ | Semua screens rebuilt, old apps ke .bak, HelloApp demo |

---

## Detail Per Plan

### Plan 47 — App Platform Overview

Umbrella plan. Tidak punya implementasi sendiri — direalisasi lewat semua plan di bawahnya.
Semua konsep (IDL, SDK, process model, window, runtime) sudah terwujud.

---

### Plan 48 — Nema System API IDL

**IDL parser & type system** di `packages/idl/`:

- `idl.ts` — parser IDL → AST (functions, events, properties, namespaces)
- `codegen.ts` — TypeScript type definitions + C++ bridge skeleton
- `gen-bindings.ts` — CLI driver, reads `nema.idl` → `nema_bindings.ts` + `js_api.cpp`
- `nema.idl` — IDL definition: namespaces `ui`, `apps`, `net`, `sys`, `fs`

Output `js_api.cpp` di-generate otomatis dan checked-in ke repo.

**Files:**
- `packages/idl/idl.ts`
- `packages/idl/codegen.ts`
- `packages/idl/gen-bindings.ts`
- `packages/idl/nema.idl`
- `firmware/core/src/js/js_api.cpp` ← generated output

---

### Plan 49 — SDK Binding Generator

**JS app SDK** di `packages/app-sdk/` (rename dari `packages/nema-app-sdk/`):

- Binding TypeScript → QuickJS bridge via `nema.idl` codegen
- Builder: `bun run build` → `dist/<id>.papp/` folder (bukan binary lagi)
- `tsconfig`, `package.json` pakai nama `@palanu/app-sdk`
- Import path: `"nema"` (bukan `"kairo"`)

**Files:**
- `packages/app-sdk/src/index.ts`
- `packages/app-sdk/src/types.ts`
- `packages/app-sdk/build.ts`
- `packages/app-sdk/package.json`

---

### Plan 50 — UI SDK Model

Node tree UI: `NodeArena` → `UiNode` → layout → renderer.

**Node types di `node.h`:**
```
NodeType: Container, Pressable, Text, ScrollView, Icon (Plan 53)
TextRole: Body, Caption, Title, Smart (Plan 52)
FlexDir, Align, Justify, Style
UiNode: type, role, style, text, onPress, userdata, focusable
         iconBitmap, iconW, iconH (Plan 53)
```

**Widgets di `widgets.h` / `widgets.cpp`:**
```
View(), Text(), Pressable(), ListRow(), ScrollView()
TitleBar()     → Plan 60
ListItem()     → Plan 60
SmartLabel()   → Plan 52
Icon()         → Plan 53
```

**Layout (`layout.cpp`):** flex layout, measure pass + place pass. NodeType::Icon
diukur sebagai leaf fixed size `iconW×iconH + 2×padding`.

**Renderer (`renderer.cpp`):**
- `setRenderTick(ms)` — dipanggil GuiService tiap frame untuk animasi marquee
- `paint(node, canvas, focused, inFocused)` — `inFocused` dipropagasi ke descendants
  sehingga SmartLabel di dalam Pressable yang focused akan marquee-scroll

---

### Plan 51 — Display Server Architecture

**Dynamic server registration** di GuiService:

```cpp
// gui_service.h
void registerServer(IDisplayServer* s);     // ← BARU
std::vector<IDisplayServer*> extraServers_; // ← BARU

// gui_service.cpp
void GuiService::registerServer(IDisplayServer* s) {
    extraServers_.push_back(s);
}
```

`findServer()` dan `serverNames()` kini juga mengecek `extraServers_` setelah server
utama, jadi WASM overlay atau test server bisa register diri sendiri.

**Files diubah:**
- `firmware/core/include/nema/services/gui_service.h`
- `firmware/core/src/services/gui_service.cpp`

---

### Plan 52 — Aether UI SDK

**SmartLabel** — text yang cerdas:
- `TextRole::Smart` ditambahkan ke enum di `node.h`
- `SmartLabel(arena, text)` → `Text(arena, text, TextRole::Smart)` di `widgets.cpp`
- Ketika `inFocused=false`: `draw::ellipsis()` — terpotong + "…"
- Ketika `inFocused=true`: `draw::marquee()` — scroll kiri-kanan animasi
- `inFocused` dipropagasi downward lewat parameter `paint()` internal renderer

**TitleBar & ListItem** (Plan 60 tapi didefinisi di Plan 52 scope):
- `TitleBar(arena, title)` — row penuh lebar, teks besar, border bawah
- `ListItem(arena, label, accessory)` — row `label [   ] accessory`
- `ListRow()` — pressable list item dengan callback

**Screens yang sudah pakai sistem baru (semua 13 screens):**
`HomeScreen`, `AppListScreen`, `SettingsScreen`, `LogsScreen`, `LockScreen`,
`AboutScreen`, `ProfileSettingsScreen`, `SleepSettingsScreen`,
`SoundsSettingsScreen`, `ControlsScreen`, `DeveloperScreen`,
`CloseAndOpenModal`, `CameraSettingsScreen`

---

### Plan 53 — Theming & Asset Packs

**Icon Pack** — 16 icon XBM 8×8 built-in:

| Handle | Deskripsi |
|--------|-----------|
| `status.wifi` | WiFi signal |
| `status.bt` | Bluetooth |
| `status.battery` | Baterai |
| `status.charging` | Charging bolt |
| `feature.apps` | Apps grid |
| `feature.settings` | Gear/settings |
| `feature.gpio` | Pin/GPIO |
| `feature.subghz` | Wireless wave |
| `feature.nfc` | NFC tap |
| `file.folder` | Folder |
| `file.file` | File page |
| `file.generic` | Generic file |
| `action.warning` | Exclamation |
| `action.info` | Info circle |
| `action.ok` | Checkmark |
| `action.spinner` | Spinner |

```cpp
// icon_pack.h
const IconDef* findIcon(const char* handle);  // linear search by strcmp
const IconDef* const* allIcons();             // iterate all
```

**Style Tokens (`style_tokens.h` / `style_tokens.cpp`):**
```cpp
struct ThemeTokens {
    struct { uint8_t xs, sm, md, lg; } space;
    struct { uint8_t sm, md, lg; }     radius;
    struct { uint8_t sm, md, lg; }     font;
    uint8_t focusIndicatorWidth;
};
const ThemeTokens& theme();
```

**StatusBar** (`status_bar.cpp`) kini pakai `findIcon("status.wifi")` untuk WiFi
indicator — XBM bitmap lewat `draw::icon()`.

**Files baru:**
- `firmware/core/include/nema/ui/icon_pack.h`
- `firmware/core/src/ui/icon_pack.cpp`

---

### Plan 54 — Process Model

**ProcessContext** — abstraksi process stdio/args/exit:
```cpp
// process_context.h
class ProcessContext {
    virtual Span<const char*> args() const = 0;
    virtual Stream& stdin_stream()  = 0;
    virtual Stream& stdout_stream() = 0;
    virtual void    exit(int code)  = 0;
};
```

**ProcessManager** — lifecycle management:
```cpp
// process_manager.h
class ProcessManager {
    ProcessHandle launch(IApp& app, ProcessContext& ctx);
    void          kill(ProcessHandle h);
    int           lastExitCode() const;
};
```

**Pipe** — SPSC ring buffer untuk pipeline `A | B`:
```cpp
// pipe.h
class Pipe {
    bool write(uint8_t b);
    bool read(uint8_t& b);
};
```

**CLI enhancements:**
- `run <app> [args...]` — launch app sebagai foreground process
- `echo $?` — exit code proses terakhir
- PATH auto-resolve: ketik nama app → langsung launch (tanpa `run`)
- `CliSession.lastExit`, `CliSession.path`
- Baris PROCESSES di output `ps`

**Files baru:**
- `firmware/core/include/nema/proc/process_context.h`
- `firmware/core/include/nema/proc/process_host.h`
- `firmware/core/src/proc/process_host.cpp`
- `firmware/core/include/nema/proc/process_manager.h`
- `firmware/core/src/proc/process_manager.cpp`
- `firmware/core/include/nema/proc/pipe.h`
- `firmware/core/src/proc/pipe.cpp`
- `firmware/core/include/nema/proc/stream.h`
- `firmware/core/src/proc/stream.cpp`

---

### Plan 55 — Surface & Window Model

**ISurface** — abstract surface (app window):
```cpp
// surface.h
class ISurface {
    virtual void        render(Canvas&) = 0;
    virtual const char* id()     const  = 0;
    virtual bool        active() const  = 0;
};
```

**IWindowPolicy** — siapa yang kelihatan:
```cpp
// window_policy.h
class IWindowPolicy {
    virtual void onSurfaceCreated(ISurface& s)   = 0;
    virtual void onSurfaceDestroyed(ISurface& s) = 0;
    virtual void visibleSurfaces(std::vector<ISurface*>& out) = 0;
    virtual ISurface* focused() = 0;
};
```

**SingleForegroundPolicy** — implementasi konkret:
- Satu surface foreground aktif pada satu waktu
- `onSurfaceCreated(s)` → simpan sebagai `foreground_`
- `onSurfaceDestroyed(s)` → hapus jika match
- `visibleSurfaces()` → push foreground jika ada
- `focused()` → return foreground

**Files baru:**
- `firmware/core/include/nema/ui/single_foreground_policy.h`
- `firmware/core/src/ui/single_foreground_policy.cpp`

---

### Plan 56 — App Runtime Architecture

**AppHost** — container per-app process:
- Implementasi `ProcessContext` untuk satu app
- Menyimpan args, stdio streams, exit code

**AppHostManager** — koordinasi multi-app:
- Launch / terminate AppHost
- Forward ke IWindowPolicy untuk surface management

**IAppRuntime** — interface tier runtime:
```cpp
class IAppRuntime {
    virtual RuntimeTier tier() = 0;
    virtual bool canRun(const AppManifest& m) = 0;
    virtual void launch(IApp& app, AppHost& host) = 0;
};
```

**BuiltinRuntime** — tier untuk C++ apps built-in.

**Files baru/diubah:**
- `firmware/core/include/nema/app/app_host.h`
- `firmware/core/src/app/app_host.cpp`
- `firmware/core/include/nema/app/app_host_manager.h`
- `firmware/core/src/app/app_host_manager.cpp`
- `firmware/core/include/nema/app/app_runtime.h`
- `firmware/core/include/nema/app/builtin_runtime.h`
- `firmware/core/include/nema/app/runtime_tier.h`

---

### Plan 57 — WASM Runtime

**wasm3** di-vendor ke `firmware/vendor/wasm3/` — dual-build: host CMake + ESP-IDF component.

**WasmEngine** — thin wrapper atas wasm3:
```cpp
class WasmEngine {
    bool init();
    bool load(const uint8_t* wasm, size_t len);
    bool runStart();  // calls _start / main
};
```

**WasmRuntime : IAppRuntime** — adapter tier:
- `tier()` → `RuntimeTier::Wasm`
- `canRun(m)` → `m.runtime == "wasm"`
- `launch()` → alokasi WasmEngine, load, runStart

**WASI stubs** (`wasm_wasi.cpp`) — 5 imports:
- `args_get`, `args_sizes_get` → dari ProcessContext
- `fd_read`, `fd_write` → stdin/stdout ProcessContext streams
- `proc_exit` → ProcessContext::exit()

**NemaResult\<T,E\>** — generic result type ditambahkan ke codebase.

> Fase 4–6 (System API gating, aether:ui surface, memory quota) — deferred.

**Files baru:**
- `firmware/vendor/wasm3/` (vendored library)
- `firmware/core/include/nema/wasm/wasm_engine.h`
- `firmware/core/src/wasm/wasm_engine.cpp`
- `firmware/core/include/nema/wasm/wasm_runtime.h`
- `firmware/core/src/wasm/wasm_runtime.cpp`
- `firmware/core/src/wasm/wasm_wasi.cpp`

---

### Plan 58 — JS Runtime Process Model

**HostApi abstraction** di JsEngine:
```cpp
class HostApi {
    virtual void install(JSContext* ctx, JSValue global) = 0;
    virtual ~HostApi() = default;
};
// JsEngine: setHostApi(HostApi*), hostApi()
```

**NemaHostImpl : HostApi** — installs nema.* API ke QuickJS global.

**JS `process` object:**
- `process.args` — array argumen
- `process.exit(code)` — memanggil ProcessContext::exit()
- `process.env` — env vars (kosong saat ini)

**`installNemaApi()`** di `js_api.cpp` — auto-generated dari IDL:
- `nema.ui.*` — UI node creation, layout control
- `nema.apps.*` — app registry queries
- `nema.sys.*` — system info, restart/shutdown
- `nema.fs.*` — filesystem R/W
- `nema.net.*` — WiFi scan/connect

**Bug fix ESP32 (Xtensa):**
- `int code` → `int32_t code` (Xtensa: `int32_t = long`, bukan `int`)
- `%d` → `%ld` dengan cast `(long)code` untuk `-Werror=format`

**Files diubah:**
- `firmware/core/include/nema/js/js_engine.h`
- `firmware/core/src/js/js_engine.cpp`
- `firmware/core/src/js/js_api.cpp`
- `firmware/core/src/js/nema_host_impl.cpp` (baru)

---

### Plan 59 — App Manifest & Packaging

**`.papp` folder format** (macOS `.app` style):
```
my-app.papp/
├── manifest.json     ← required
├── app.js            ← entry point
├── icons/            ← optional assets
└── ...
```

**`manifest.json` fields:**
```json
{
  "id": "com.example.myapp",
  "name": "My App",
  "version": "1.0.0",
  "runtime": "js",
  "display_server": "",
  "category": "GPIO",
  "iconPath": "feature.gpio"
}
```

**`loadInstalledPapps(rt)`** — scan `/apps` + `/sd/apps` recursive:
1. Scan semua `.papp` folder dan `.papp`/`.kapp` file
2. Bandingkan dengan cache `s_installedIds` (unordered_set)
3. Install yang baru, uninstall yang hilang, skip yang sama
4. Hot-reload: panggil tiap kali `AppListScreen::enter()`

**`rt.setFs()` / `rt.fs()`** — Runtime sekarang expose filesystem:
```cpp
// runtime.h
void          setFs(IFileSystem* fs) { fs_ = fs; }
IFileSystem*  fs()                   { return fs_; }
```
Platform (ESP32, WASM) panggil `rt.setFs(&vfs_)` saat init.

**Binary PAPP1 tetap didukung** untuk PLP wire transfer:
- Magic `"PAPP1\n"` → parse TOC → explode ke `/apps/<id>.papp/`
- Magic `"KAPP1\n"` → delegate ke `JsAppStore::installKapp()`

**Files baru:**
- `firmware/core/include/nema/app/papp_installer.h`
- `firmware/core/src/app/papp_installer.cpp`
- `firmware/core/include/nema/app/papp_package.h`
- `firmware/core/src/app/papp_package.cpp`

**Files diubah:**
- `firmware/core/include/nema/runtime.h` — tambah `setFs`, `fs_`
- `firmware/platforms/esp32/src/esp32_platform.cpp` — `rt.setFs(&vfs_)`
- `firmware/platforms/wasm/src/wasm_platform.cpp` — `rt.setFs(&vfs_)`

---

### Plan 60 — Aether UI Rewrite

**Cleanup: old apps ke `.bak/`**

10 built-in C++ apps dipindahkan ke `.bak/aether-v1/apps/`:
```
bluetooth_app.cpp   camera_app.cpp     clock_app.cpp
counter_app.cpp     stopwatch_app.cpp  task_demo_app.cpp
ticker_app.cpp      touch_test_app.cpp ui_showcase_app.cpp
wifi_app.cpp
```
Beserta header mereka di `.bak/aether-v1/apps-include/`.

Old UI/screen code (sebelum Plan 60) di `.bak/aether-v1/ui/`, `screens/`, dll.

**Yang tersisa di `firmware/core/src/apps/`:**
- `hello_app.cpp` — demo app pakai widget Plan 60
- `js_app.cpp` — adapter JS app ke ComponentScreen
- `js_app_store.cpp` — install/launch JS apps

**AppListScreen (Plan 60 layout):**
```
┌─ APPS ──────────────────────────┐
│ [icon] App Name             [>] │  ← SmartLabel flexGrow=1
│ [icon] Very Long App Name…  [>] │  ← ellipsis (unfocused)
│ [icon] Very Long App Name→  [>] │  ← marquee scroll (focused)
└─────────────────────────────────┘
```
- Icon 8×8 XBM dari `icon_pack` berdasar category atau `iconPath` di manifest
- `SmartLabel` dengan `flexGrow=1` untuk nama app
- ">" sebagai accessory

**All 13 screens rebuilt** pakai sistem baru:
- `TitleBar` + `ListItem` / `ListRow`
- Tidak ada hardcoded dimensions
- `hintFor(Action)` untuk label tombol

**HelloApp** — demo clean yang menunjukkan:
- `TitleBar`
- `SmartLabel`
- `ListItem`
- Toggle (via Pressable)

---

## Files Baru (Session 2)

| File | Plan |
|------|------|
| `firmware/core/include/nema/ui/icon_pack.h` | 53 |
| `firmware/core/src/ui/icon_pack.cpp` | 53 |
| `firmware/core/include/nema/ui/single_foreground_policy.h` | 55 |
| `firmware/core/src/ui/single_foreground_policy.cpp` | 55 |
| `firmware/core/include/nema/app/papp_installer.h` | 59 |
| `firmware/core/src/app/papp_installer.cpp` | 59 |
| `firmware/core/include/nema/app/papp_package.h` | 59 |
| `firmware/core/src/app/papp_package.cpp` | 59 |
| `firmware/core/src/js/nema_host_impl.cpp` | 58 |
| `firmware/vendor/wasm3/` | 57 |
| `firmware/core/include/nema/wasm/wasm_engine.h` | 57 |
| `firmware/core/src/wasm/wasm_engine.cpp` | 57 |
| `firmware/core/include/nema/wasm/wasm_runtime.h` | 57 |
| `firmware/core/src/wasm/wasm_runtime.cpp` | 57 |
| `firmware/core/src/wasm/wasm_wasi.cpp` | 57 |
| `firmware/core/include/nema/proc/process_context.h` | 54 |
| `firmware/core/include/nema/proc/process_host.h` | 54 |
| `firmware/core/src/proc/process_host.cpp` | 54 |
| `firmware/core/include/nema/proc/process_manager.h` | 54 |
| `firmware/core/src/proc/process_manager.cpp` | 54 |
| `firmware/core/include/nema/proc/pipe.h` | 54 |
| `firmware/core/src/proc/pipe.cpp` | 54 |
| `firmware/core/include/nema/proc/stream.h` | 54 |
| `firmware/core/src/proc/stream.cpp` | 54 |
| `firmware/core/src/apps/hello_app.cpp` | 60 |
| `firmware/.bak/aether-v1/` | 60 |
| `packages/app-sdk/` | 49 |
| `packages/idl/` | 48 |
| `examples/hello/` | 59 |
| `examples/deauth/` | 59 |

## Files Diubah (Session 2)

| File | Perubahan |
|------|-----------|
| `firmware/core/include/nema/ui/node.h` | `TextRole::Smart`, `NodeType::Icon`, `iconBitmap/W/H` |
| `firmware/core/include/nema/ui/widgets.h` | deklarasi `SmartLabel()`, `Icon()` |
| `firmware/core/src/ui/widgets.cpp` | implementasi `SmartLabel()`, `Icon()` |
| `firmware/core/src/ui/layout.cpp` | `NodeType::Icon` → fixed-size leaf |
| `firmware/core/src/ui/renderer.cpp` | `inFocused` propagation, SmartLabel marquee/ellipsis, Icon render, `setRenderTick` |
| `firmware/core/include/nema/services/gui_service.h` | `registerServer()`, `extraServers_` |
| `firmware/core/src/services/gui_service.cpp` | `registerServer()` impl, `findServer()` check extraServers, `setRenderTick()` call |
| `firmware/core/src/ui/status_bar.cpp` | WiFi icon pakai `findIcon("status.wifi")` + `draw::icon()` |
| `firmware/core/include/nema/screens/app_list_screen.h` | `icons_` vector |
| `firmware/core/src/screens/app_list_screen.cpp` | icon+SmartLabel row, `loadInstalledPapps` on enter |
| `firmware/core/include/nema/runtime.h` | `setFs()`, `fs()`, `IFileSystem* fs_` |
| `firmware/platforms/esp32/src/esp32_platform.cpp` | `rt.setFs(&vfs_)` |
| `firmware/platforms/wasm/src/wasm_platform.cpp` | `rt.setFs(&vfs_)`, VFS seed paths |
| `firmware/core/include/nema/js/js_engine.h` | `HostApi*`, `setHostApi()`, `hostApi()` |
| `firmware/core/src/js/js_engine.cpp` | delete hostApi_ di dtor |
| `firmware/core/src/js/js_api.cpp` | generated bridge, fix `int32_t`, fix `%ld` |
| `firmware/core/src/screens/*.cpp` | semua 13 screens rebuilt (TitleBar, ListItem) |
| `firmware/core/CMakeLists.txt` | tambah papp_installer, icon_pack, single_foreground_policy, wasm, proc files |

---

## Build Status

| Target | Status | Catatan |
|--------|--------|---------|
| Host (Linux/macOS) | ✅ clean | 10/10 tests pass |
| ESP32 (Xtensa) | ✅ clean | setelah fix int32_t + %ld |
| WASM simulator | ✅ clean | |
| Bun parser | ✅ 24/24 pass | IDL + app-sdk tests |

---

## Bug Fixes

**ESP32 Xtensa: `int` vs `int32_t`** (`js_api.cpp`)
- `JS_ToInt32(ctx, &code, argv[0])` butuh `int32_t*`, bukan `int*`
- Pada Xtensa: `int32_t = long`, `int ≠ long`
- Fix: `int code` → `int32_t code`

**ESP32 Xtensa: format specifier** (`js_api.cpp`)
- `printf("process.exit(%d)", code)` → `-Werror=format` karena `%d` untuk `long`
- Fix: `"process.exit(%ld)"` + cast `(long)code`
