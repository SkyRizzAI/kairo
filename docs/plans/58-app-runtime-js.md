# 58 — App Runtime: JS Tier (QuickJS-ng)

> Runtime app mainstream untuk developer JS modern: **QuickJS-ng** (di-harden),
> global `process`/`nema`/`aether/ui`, authoring TSX → node-tree.

- Status: 🟧 Detail draft (belum diimplementasi)
- Depends on: 49 (SDK gen), 54 (process), 55 (surface), 56 (arch)
- Blocks: 59 (manifest/packaging)

---

## Goals

- Integrasi **QuickJS-ng** (bukan mJS — perlu JS modern beneran untuk DX).
- Hardening produksi: heap bounded per-app, thread terisolasi, watchdog/timeout,
  bunuh app runaway (mengatasi freeze/stack-overflow prototype lama).
- Binding: `process` (stdio/argv/exit), `nema` (System API), `aether/ui` (node-tree).
- SDK authoring TSX (reify → UiNode), reuse pipeline build bun.

## Keputusan

- **QuickJS-ng**, bukan mJS/AssemblyScript — goal-nya familiaritas JS modern.
- JS engine = **native di core** (bukan QuickJS-on-wasm3 → terlalu lambat di ESP32).
- Binding = adapter JS dari Plan 56 di atas Host API yang sama.

---

## Latar belakang

**Penting — tier JS BUKAN greenfield.** QuickJS-ng sudah ter-integrasi sebagai
*prototype* lewat Plan 37 (Fase 1–8 ✅): engine vendored, bridge JS→UiNode jalan,
app embedded + OTA `.kapp` jalan, system API `nema.*` capability-gated. Plan 58
**bukan** "pasang engine"; ia **menaikkan prototype itu jadi tier resmi** di bawah
kontrak adapter Plan 56 + **menyelesaikan hardening** yang membuat prototype lama
freeze.

**Yang sudah ada (Plan 37):**

- **Engine vendored**: `firmware/vendor/quickjs/` (QuickJS-ng, MIT, dual-build —
  pola `if(ESP_PLATFORM) idf_component_register else add_library`). ES2020 penuh,
  Promise/GC bawaan, interrupt handler.
- **`JsEngine`** (`core/src/js/js_engine.cpp`, `core/include/nema/js/js_engine.h`):
  satu `JSRuntime`+`JSContext` per app; `eval`, error capture, `setMemoryLimit`,
  `setMaxStackSize`, `setDeadlineMs` (interrupt), module loader yang me-resolve
  `nema` → runtime tertanam (`nema_runtime_js.h`), `reify()` JS node-desc→`UiNode`
  + handler table.
- **`JsApp : ComponentApp`** (`core/src/apps/js_app.cpp`): `build()` =
  `reify(callComponent())` → otomatis dapat scroll/gesture/focus/pause Plan 22.
  Stack override **256 KB di ESP32 / 512 KB host** (`apps/js_app.h:39,41`).
- **System API `nema.*`** (`core/src/js/js_api.cpp:117-166`): `log`,
  `device{name,caps,has,available}`, `storage` (per-app ns), `http.get` (blocking
  di thread app, off UI), `profile.*` — semua capability-gated.
- **App store**: `.kapp` container (`KAPP1\n<manifest>\n<bundle>`); `JsAppStore`
  (`core/src/apps/js_app_store.cpp`) `installApp`/`installKapp`/
  `loadEmbeddedJsApps`; OTA via PLP `ExtOp::AppInstall` (volatile RAM, Plan 35/37).
- **DX**: `packages/nema-app-sdk` — TSX (`jsxImportSource`) → `bun build` → satu
  `.kapp`; hooks `useState/useRef/useEffect`; intrinsics View/Text/Pressable/Scroll/
  Slider/Row/Col (Plan 37 Fase 0/8).

**Masalah prototype lama (freeze/stack-overflow) — DIAGNOSA & status fix:**

QuickJS *recurse dalam* saat parse/eval (module evaluation = dispatch interpreter
rekursif). Pada stack **256 KB PSRAM** (`js_app.h:39`), dua kegagalan muncul:

| Gejala | Akar masalah | Fix (sudah landed di prototype) | Sumber |
|---|---|---|---|
| **Device freeze** | Stack overflow **meng-korup TCB FreeRTOS** alih-alih melempar — OS beku, tak recover | `setMaxStackSize(stackBytes()*3/4)` — recursion-guard QuickJS **dikoordinasi** dengan stack thread asli → script terlalu-dalam *throw* `RangeError` bersih | `apps/js_app.cpp:29` |
| **Sim "Maximum call stack size exceeded"** | Guard QuickJS dibiarkan default 1 MB, tak nyambung dengan stack worker WASM | guard di-set di bawah stack riil **tiap platform** | `apps/js_app.cpp:25-29` |
| **Spike saat load** | Parse runtime minified (nested dalam) + app module = puncak stack = parse+depth | **preload runtime di kedalaman dangkal** sebelum turun ke app → puncak = max(parse, depth), bukan jumlah | `js_engine.cpp` (`preloadRuntime`) |
| **Loop tak henti** | Tak ada deadline | `setDeadlineMs(5000)` interrupt per turn | `js_engine.cpp:34-37`, `apps/js_app.cpp:30` |
| **OOM** | Heap tak dibatasi | `setMemoryLimit(4 MB)` (PSRAM) | `apps/js_app.cpp:22` |

`js_graceful_test.cpp` membuktikan kontrak **"app boleh gagal, OS tidak"**: parse
20k array bersarang & rekursi tak-henti → *throw* bersih (engine survive), bukan
korup stack. **Jadi fix utama sudah ada** — Plan 58 mem-*formalkan*-nya jadi
kontrak tier + menutup sisa celah.

**Celah yang ditutup Plan 58 (di atas prototype):**

1. **Re-base ke Host API Plan 56**: prototype memanggil `rt` langsung; Plan 58
   menjadikan `JsApp`/`JsEngine` **adapter JS** (`JsRuntime : IAppRuntime`) di atas
   `ProcessContext`/`ISurface`/System API (`nema:<domain>/<name>`)/`aether:ui` yang sama dengan C & WASM.
2. **Binding `process`** (Plan 54 — **belum ada**; prototype cuma `nema.*`):
   `process.argv/stdin/stdout/stderr/exit/cwd/env`.
3. **Binding `aether/ui`** sebagai modul eksplisit (Plan 50/52) — `reify` internal
   prototype diangkat jadi import resmi (sejajar `aether:ui` WASM).
4. **Hardening tingkat-OS**: watchdog yang **mem-bunuh app yang thread-nya tak
   balik** (mis. loop di dalam host-call native) — di atas deadline per-eval; +
   isolasi heap/thread per-app sebagai *kontrak*, bukan kebetulan.

### Pelajaran dari referensi

| Aspek | Flipper Momentum | AkiraOS (Zephyr) | Keputusan Palanu |
|---|---|---|---|
| Engine JS | **mJS** (`applications/system/js_app/js_thread.c:242` `mjs_create`) — ES5.1 *subset*, FFI ke C | **Tak ada JS** (WASM-only, Plan 57) | **QuickJS-ng** — ES2020 penuh, modul, Promise/GC |
| DX bahasa | mJS = bukan JS beneran (no `let`/class/async, no modules) | — | JS modern → **TSX/React-style** (Plan 37 SDK) |
| Binding | `JS_FIELD("print"/"delay"/"require"/"ffi_address")` (`js_thread.c:267-271`) | — | `process` (stdio) + `nema` (sys) + `aether/ui` (node-tree) |
| Eksekusi | `FuriThread` per-script | — (WASM) | Thread `ProcessHost` per-app (Plan 56), heap PSRAM |
| Runaway/crash | Watchdog furi | `app_manager` state | **Stack-guard dikoordinasi + deadline interrupt + watchdog kill** (fix freeze) |
| FFI/keamanan | `ffi_address` = **panggil C arbitrer** (tak sandboxed) | WASM sandbox | **Capability-gated `nema.*`**, tak ada FFI mentah |

Flipper memilih **mJS** justru karena ringan, tapi membayarnya di DX: bukan JS
beneral (ES5.1 subset, tanpa modul/async/class) dan `ffi_address` membuka panggil
C mentah (nol sandbox). Palanu **menolak** trade itu — goal kita adalah DX
developer JS modern (TSX, async, npm-bundling via bun), jadi **QuickJS-ng** wajib.
Konsekuensinya engine lebih besar & recurse lebih dalam (sumber freeze prototype)
— dijawab dengan hardening (stack-guard terkoordinasi + deadline + watchdog), bukan
dengan turun ke mJS. AkiraOS tak punya JS sama sekali (WASM-only) — Palanu sengaja
menyediakan **dua** tier scripting (WASM low-level Plan 57, JS mainstream di sini).

---

## Desain teknis

### 1. `JsRuntime : IAppRuntime` — adapter JS (Plan 56)

```cpp
// JS jadi tier ketiga di RuntimeRegistry (sejajar CBuiltin & Wasm).
struct JsRuntime : IAppRuntime {
    RuntimeTier tier() const override { return RuntimeTier::Js; }
    bool canLoad(const AppManifest& m) const override;          // bukan \0asm, ada bundle JS
    IApp* instantiate(Runtime&, const AppManifest&,
                      const uint8_t* pkg, size_t) override;      // → JsApp (bungkus JsEngine)
    void destroy(IApp*) override;
    // forceTerminate (Plan 56 §5): arm interrupt → JS_Eval balik, dispose JSRuntime.
};
```

`JsApp::run(ProcessContext& ctx)` (migrasi dari `ComponentApp` lama): `JsEngine`
init (heap PSRAM, stack-guard, deadline) → `loadApp(bundle)` → loop render via
`Surface` (Plan 55). Lifecycle thread/exit/pause/kill **dari `ProcessHost`** — sama
persis C & WASM. **Engine = native di core**, bukan QuickJS-on-wasm3 (interpret di
atas interpret = terlalu lambat di ESP32 — keputusan terkunci).

### 2. Tiga binding di atas Host API yang sama

**a. `process` (Plan 54 — global ambient, BARU).** Objek yang men-delegasi ke
`ProcessContext`:

```js
process.argv          // ctx.args()
process.env(k)/cwd()  // ctx.env()/cwd()
process.stdin         // EventEmitter byte → ctx.in().read()
process.stdout/stderr // .write(s) → ctx.out()/err().write()
process.exit(code)    // ctx.requestExit(code)
```

Disuntik sebagai global C-function (`JS_SetPropertyStr`, pola `js_api.cpp:20-26`).
Parser arg (commander/yargs) = paket npm biasa **di dalam `.kapp`**, bukan device
(Plan 54 §6).

**b. `nema` (Plan 48 — System API, SUDAH ADA).** JS path kanonik
`nema.<domain>.<name>` (skema Plan 48 §2): `nema.sys.log`, `nema.sys.device`,
`nema.storage.kv`, `nema.net.http`, … (prototype `js_api.cpp:117-166` masih pakai
bentuk datar lama `nema.http`/`nema.storage` → di-alias deprecated satu rilis,
Plan 48). Capability-gated: hanya fungsi yang board+manifest
grant yang disuntik (app tanpa grant → properti absen). Kerja blocking (http/wifi)
jalan di thread app (off UI) atau via TaskRunner → resolve Promise; event-loop
QuickJS dipompa (`JS_ExecutePendingJob`) tiap iterasi loop.

**c. `aether/ui` (Plan 50/52 — modul, formalisasi `reify`).** `import { View,
Text, Pressable, useState } from "aether/ui"` (rename dari `nema` runtime
prototype). Komponen → node-desc (objek JS) → `JsEngine::reify` →
`UiNode`/`NodeArena` + handler table (`onPress` thunk →
`engine.callHandler(id)`). Surface diangkat lewat `createSurface` (Plan 55) — app
JS UI = ComponentApp di atas `ISurface`; app JS headless (cuma `process`+`nema`)
tak mengangkat surface.

### 3. Hardening produksi — kontrak tier (bukan kebetulan)

| Pagar | Mekanisme | Status |
|---|---|---|
| **Heap bounded per-app** | satu `JSRuntime`/app, `JS_SetMemoryLimit` (PSRAM, default 4 MB, override manifest) | ada (`js_app.cpp:22`) — formalkan per-manifest |
| **Thread terisolasi** | satu thread `ProcessHost`/app; GC/eval tak ganggu UI | ada (Plan 22) |
| **Stack guard terkoordinasi** | `JS_SetMaxStackSize(stackBytes()*3/4)` per platform → throw bukan korup | ada (`js_app.cpp:29`) |
| **Preload dangkal** | runtime di-eval sebelum app → puncak stack ditekan | ada (`js_engine.cpp`) |
| **Deadline per-turn** | `JS_SetInterruptHandler` deadline 5 s → hentikan eval/loop | ada (`js_engine.cpp:34-37`) |
| **Watchdog kill (OS)** | `ProcessHost::kill`→`JsRuntime::forceTerminate`: arm interrupt agar `JS_Eval` balik + dispose `JSRuntime` dari thread watchdog | **BARU** (Plan 56 §5) |
| **No FFI mentah** | tak ada `ffi_address` ala mJS; hanya `nema.*` gated | desain (kontra-Flipper) |

Watchdog OS menutup celah terakhir: deadline interrupt menangkap loop **JS**, tapi
app yang menggantung di **host-call native** (mis. http hang) butuh kill dari luar
— `forceTerminate` (Plan 56) yang membongkar `JSRuntime`; thread+heap per-app
terisolasi → bunuh satu tak sentuh lain.

### 4. Pipeline authoring TSX → `.kapp` (bun)

`packages/nema-app-sdk` (sudah ada, Plan 37 Fase 0/8): `App.tsx`
(`jsxImportSource: "aether/ui"`) → `bun build` (bundle ES2020, minified, tree-shaken,
runtime `aether/ui` external/ambient) → satu `.kapp` (`KAPP1\n<manifest>\n<bundle>`).
Reify JS node-desc → `UiNode` di device (`js_engine.cpp`). Install: embed ke
firmware (`loadEmbeddedJsApps`) **atau** OTA PLP (`installKapp`, volatile RAM).
Plan 58 menyelaraskan SDK ke binding baru (`process` + rename `nema`→`aether/ui`
untuk modul UI; `nema` tetap untuk System API).

> **Container/packaging** (`.kapp`/KAPP1 → `.papp`/PAPP1) diformalkan di **Plan 59**:
> manifest `manifest.json`, format transfer PAPP1 (TOC, bukan zip), launcher/install.
> Plan 58 hanya soal *tier eksekusi JS*; bentuk paket yang dikirim = `.papp` (Plan 59).

---

## Fase

> Prototype Plan 37 = titik awal (Fase 0 efektif sudah ✅). Fase di bawah =
> *delta* untuk menaikkan ke tier resmi + hardening penuh.

- [ ] **Fase 1 — `JsRuntime:IAppRuntime` (sinkron Plan 56).** Bungkus `JsApp`/
      `JsEngine` jadi `IAppRuntime`; daftar ke `RuntimeRegistry`; `canLoad` (bukan
      `\0asm`). Parity: app JS embedded/`.kapp` yang ada jalan identik lewat jalur
      tier. Host+WASM test.
- [ ] **Fase 2 — `JsApp::run(ProcessContext&)` + Surface (sinkron Plan 54/55).**
      Migrasi dari `ComponentApp`/`AppContext` ke `ProcessContext` + `createSurface`.
      App JS UI angkat surface; app JS headless tidak. Parity visual host+WASM.
- [ ] **Fase 3 — Binding `process` (sinkron Plan 54 Fase 5).** Inject global
      `process` (argv/stdin/stdout/exit). Smoke `.kapp` headless: echo argv, baca
      stdin, exit code; pipe `jsapp | other` lintas-tier.
- [ ] **Fase 4 — Modul `aether/ui` + SDK realign.** Rename runtime UI →
      `aether/ui`; `reify` jadi import resmi; `nema` tetap System API. `bun build`
      SDK + contoh diperbarui. Test: reify View/Text/Pressable + onPress + useState.
- [ ] **Fase 5 — Hardening penuh: watchdog kill + quota per-manifest.**
      `forceTerminate` (arm interrupt + dispose `JSRuntime`) dari `ProcessHost::
      kill`; heap limit dari manifest. Test: app loop tak-henti & app menggantung di
      host-call → di-kill, OS + app lain hidup (lanjutan `js_graceful_test`).

**Build/uji:** host + WASM tiap fase; ESP32 build-only Fase 1, 2, 5 (footprint +
stack PSRAM).

---

## File yang disentuh

- **Baru:** `core/src/js/js_runtime.cpp` (`JsRuntime:IAppRuntime` +
  `forceTerminate`), `core/src/js/js_process.cpp` (binding global `process` →
  `ProcessContext`).
- **Diubah:** `core/src/apps/js_app.cpp` + `apps/js_app.h`
  (`run(ProcessContext&)`, surface lewat `createSurface`, daftar ke
  `RuntimeRegistry`), `core/src/js/js_engine.cpp` + `include/nema/js/js_engine.h`
  (modul `aether/ui`; hook `forceTerminate`/interrupt untuk watchdog),
  `core/src/js/js_api.cpp` (`nema.*` tetap; pisahkan dari `aether/ui`),
  `core/src/apps/js_app_store.cpp` (manifest quota; `installCustom`→`runtime=Js`),
  `runtime.cpp` (daftar `JsRuntime`).
- **SDK (Plan 37/49):** `packages/nema-app-sdk` — `jsxImportSource: "aether/ui"`,
  deklarasi `process`, contoh headless + UI; `bun build` → `.kapp`.

---

## Test

- **Parity (host+WASM):** app JS embedded + `.kapp` OTA yang ada jalan identik
  lewat `JsRuntime` (render, onPress, useState re-render, pause/resume Plan 22).
- **`process` (host+WASM):** argv echo, stdin read, exit code; pipe JS↔native
  (Plan 54).
- **`aether/ui`:** reify View/Text/Pressable; onPress dispatch ke JS; useState
  Count 0→1→2; geometri dari `surface.width()/height()` (resolution-independent).
- **Capability:** `nema.net.http` tanpa grant → absen; dengan grant → fetch off-UI,
  UI tak freeze.
- **Hardening (kritis):** rekursi tak-henti & parse 20k-nested → throw bersih
  (engine survive, `js_graceful_test`); loop tak-henti → deadline interrupt;
  gantung di host-call → watchdog `forceTerminate` membunuh app, OS + app lain
  hidup; heap > limit → OOM bersih.
- **ESP32:** build-only — QuickJS footprint + stack 256 KB PSRAM + heap PSRAM
  waras; verifikasi fungsional di WASM sim / device (host tanpa platform pasca
  native-sim removal, Plan 37).

---

## Risiko & mitigasi

- **Dua engine (QuickJS + wasm3) di flash ESP32.** → QuickJS ~300–600 KB code
  (Plan 37: skyrizz N16 ~60% free dengan QuickJS saja), wasm3 ~64 KB (Plan 57) →
  muat. Heap JS di **PSRAM** (8 MB di skyrizz R8). Ukur per fase (ESP32 build-only).
- **Freeze/stack-overflow (masalah prototype).** → **Sudah ditangani**: stack-guard
  terkoordinasi (`js_app.cpp:29`) + preload dangkal + deadline + memory limit;
  `js_graceful_test` jadi gate regresi. Plan 58 menambah watchdog OS untuk
  host-call hang. Kontrak: **app boleh gagal, OS tidak**.
- **GC pause mengganggu UI.** → JS jalan di **thread app** (Plan 22), bukan UI
  thread; GC/eval tak menyentuh render. Heap per-app → GC terlokalisasi.
- **App menggantung di host-call native (deadline JS tak menangkap).** →
  `forceTerminate` (arm interrupt + dispose `JSRuntime`) dari thread watchdog;
  isolasi thread+heap per-app → bunuh satu aman.
- **Godaan QuickJS-on-wasm3 (satu engine, hemat flash).** → **Ditolak**: interpret
  JS di atas interpret WASM = terlalu lambat di ESP32 (keputusan terkunci). Engine
  JS native di core.
- **DX drift (binding `process`/`nema`/`aether/ui` vs SDK).** → SDK
  (`nema-app-sdk`) jadi sumber tipe tunggal; `bun build` + contoh jadi gate;
  rename `nema`→`aether/ui` (modul UI) didokumentasikan, `nema` tetap System API.

---

## Yang sengaja TIDAK dikerjakan (sekarang)

- **Turun ke mJS demi flash** — kalah DX (bukan JS modern); QuickJS-ng terkunci.
- **QuickJS-on-wasm3** — terlalu lambat di ESP32; engine native di core.
- **FFI mentah ala Flipper `ffi_address`** — bocor sandbox; hanya `nema.*` gated.
- **Multi-app JS paralel** — single-slot + pause (Plan 22/55).
- **Persistensi `.kapp` lintas reboot** — butuh flash-FS (Plan 37 catatan); OTA
  volatile RAM dulu.
