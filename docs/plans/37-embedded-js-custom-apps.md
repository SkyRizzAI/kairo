# 37 — Embedded JS Engine & Loadable Custom Apps (TSX)

> Developer menulis custom app pakai **TSX** (gaya React/Ink/React-Native),
> `build` → **satu file `.kapp`**, lalu **di-load di device** (Kairo). App jalan di
> **JS engine tertanam (QuickJS)**, render UI lewat **komponen native yang sama**
> (Plan 27/30/31 — satu pohon `UiNode`), dan memanggil **system API** (http, wifi,
> ble, storage, …) yang di-expose & **capability-gated**. Persis semangat custom
> apps Flipper Zero: bisa diinstal/di-load, memanfaatkan hardware.

- Status: 🚧 **IN PROGRESS** (2026-06-08). **Fase 0 ✅** — `packages/kairo-app-sdk`
  (`kairo`): jsx-runtime (auto), intrinsics (View/Text/Pressable/ScrollView/Slider/
  Row/Col), hooks (useState/useRef/useEffect), `renderToTree`, system-API types,
  `kairo-build` (Bun.build → single `.kapp`, `kairo` runtime external). Counter
  template builds → `.kapp` (718B JS); `renderToTree` host-verified (node-desc tree
  matches the bridge contract, ALL PASS).
  **Fase 1 ✅** — QuickJS-ng vendored (`firmware/vendor/quickjs`, MIT, dual-build
  component) + `JsEngine` wrapper (eval, error capture, mem-limit, deadline
  interrupt). `js_test` host ALL PASS.
  **Fase 2 ✅** — module loader (resolves `kairo` → embedded runtime header),
  `JsBridge.reify` (JS node-desc → native `UiNode` + handler table), `JsApp :
  ComponentApp`. `js_render_test` ALL PASS (reify View/Text/Pressable + onPress
  dispatch into JS).
  **Fase 3 ✅** — `useState` reactivity: onPress → re-render reflects new state
  (Count 0→1→2 host-verified). Buttons re-render via ComponentApp.
  **Fase 4 ✅** — `kairo.*` system API installed (capability-gated): `log`,
  `device{name,caps,has}`, `storage` (per-app config ns), `http.get` (blocking on
  the app thread, off UI). Wired into JsApp.
  **Fase 5 (Embedded) ✅** — `kairo-build` `.kapp` + `gen-embedded-apps` → C header;
  `JsAppPlugin` + `loadEmbeddedJsApps` register built-in JS apps in the Apps list;
  launch via AppHostManager (pause/resume free). Counter JS app embedded.
  Build green on **host (ctest 5/5), WASM, skyrizz-e32 (60% free), dev-board (41%)**.
  **Fase 8 (DX) ✅** — SDK README + 2nd example (`sysinfo`: device/storage/log +
  ScrollView). 2 embedded apps; build green host/WASM/skyrizz(60%)/dev-board.
  **Fase 6 ✅ END-TO-END (volatile, OTA does NOT need microSD)** — `JsAppStore` +
  `installKapp(rt, bytes, len)` registers a pushed `.kapp` as a live `JsAppPlugin`
  → appears in Apps **immediately, filesystem-free** (volatile, lost on reboot).
  Wired over KLP: new `ExtOp::AppInstall` (Plan 35) → `RemoteService` dispatch →
  platform `controlThunk` (wasm + esp32) → `installKapp`. Forge: `RemoteSession.
  installApp(kapp)` + a **`/install`** route (pick/paste `.kapp` → push). Same KLP
  path for WASM virtual-cable AND real BLE/USB. Builds green host/WASM/skyrizz
  (60%)/dev-board (41%); Forge `check` 0 errors.
  **Storage tiers (corrected):** volatile-RAM OTA works now (no storage); persistent
  install needs an **internal-flash FS (SPIFFS/LittleFS), NOT microSD**; microSD is
  only for bulk/removable libraries. So the only thing still pending real infra is
  *persistence across reboot* (a small flash-FS layer) + the Forge upload UI + the
  Ext-op wire. Functional verification of API/launch/install is on the WASM sim /
  device (host has no platform after the native-sim removal).
- Milestone: M10 (Custom App Ecosystem)
- Depends on: **27/30/31** (component UI + runtime + scroll/gesture),
  **33-tsx-mapping-styling** (kontrak node↔TSX + styling — di-*supersede* & diperluas
  di sini), **19.5/19.6** (Nema thread + AppHost/AppContext), **24** (config/storage),
  **34/35/36** (BLE/USB + KLP link + Forge → jalur install over-the-wire).
- Blocks: Kairo App Store, microSD app loading.

> Catatan penomoran: ada tabrakan `33` (board-profile vs tsx-mapping-styling).
> Plan 37 ini adalah rencana **implementasi** dari ide JS-engine; `33-tsx-mapping-
> styling` tetap jadi acuan **kontrak komponen/styling**.

---

## 0. Keputusan kunci & alasan

### 0.1 Engine: **QuickJS (quickjs-ng)**
| Opsi | Verdict |
|---|---|
| **QuickJS-ng** | ✅ ES2020 penuh, kecil (~300–600KB code), C bridge mudah, jalan di host/WASM/ESP32, Promise/GC bawaan, ada interrupt-handler (anti runaway) |
| JerryScript | lebih kecil tapi ES subset, bridge lebih repot |
| Elk / mJS | terlalu minim (bukan JS beneran), nggak cukup buat TSX/React-style |
| Espruino | satu firmware utuh, bukan library embeddable |

ESP32-S3 (skyrizz R8) punya **8MB PSRAM** → heap QuickJS di PSRAM. Host & WASM:
QuickJS C portable → ikut kekompilasi (penting buat tes tanpa hardware + Forge sim).

### 0.2 Bukan full React — **hyperscript + functional components**
react-reconciler + Yoga (cara Ink) terlalu berat untuk MCU. Kita pakai model yang
SUDAH ada di Kairo: **rebuild pohon tiap render** (lihat ComponentApp). Maka:
- TSX dikompilasi (`jsx: "automatic"`, `jsxImportSource: "kairo"`) → `h()` calls.
- Komponen = fungsi yang return pohon (persis `function App(){ return <View/> }`).
- State via **hooks minimal** (`useState`/`useEffect`) ATAU **signals** kecil; tiap
  perubahan → re-invoke komponen → pohon baru → bridge → `UiNode` → layout/render.
- Reuse **ComponentRuntime** → JS app otomatis dapat scroll, gesture, focus-ring,
  momentum, dan **pause/resume (Plan 22)**.

### 0.3 Satu pohon, dua produsen (dari Plan 33)
`UiNode` tetap satu-satunya representasi. JS menghasilkan **deskripsi node**
(plain object) → bridge menulisnya ke `NodeArena` jadi `UiNode`. Layout, renderer,
hit-test, scroll, focus — **tidak berubah**.

```
TSX  ──build(esbuild)──► app.kapp (JS bundle + manifest)
                              │ load
                              ▼
                    QuickJS context (app thread)
   default export App()  ──►  node-desc tree (JS objects)
                              │ JsBridge.reify()
                              ▼
                         UiNode (NodeArena)  ──► layout/render/gesture (native)
   onPress(id) ◄── C++ fires handler id ◄── input (button/touch)
   kairo.http/wifi/ble/... ◄── JS host functions ──► Runtime services (TaskRunner)
```

---

## 1. Goal (acceptance, tingkat-tinggi)

1. **DX**: `npm create kairo-app` → tulis `App.tsx` → `kairo-build` → `app.kapp`
   (satu file). Tipe TypeScript lengkap untuk komponen + system API.
2. **Load di device**: `.kapp` muncul di **Apps list**, dipilih → jalan seperti app
   native (di thread sendiri via AppHost; dapat pause/resume).
3. **System API ter-expose & capability-gated**: `kairo.log`, `kairo.http`,
   `kairo.wifi`, `kairo.ble`, `kairo.storage`, `kairo.timer`, `kairo.device`.
   App hanya bisa yang board-nya support + yang diizinkan manifest.
4. **Storage/install**: app tersimpan persisten (flash SPIFFS/NVS **sekarang**;
   microSD **nanti**) lewat `IAppStore`. **Install over-the-wire** dari Forge via
   KLP/BLE (Plan 34/35) — push `.kapp` → device simpan → muncul di Apps.
5. **Sandbox**: app tidak bisa menyentuh apa pun di luar API yang di-expose;
   ada **limit memori + interrupt** (anti loop tak henti / OOM).
6. Semua teruji di **host + WASM (Forge sim)** dulu, baru hardware.

---

## 2. Arsitektur komponen

### 2.1 `packages/kairo-app-sdk` (npm, DX layer)
```
packages/kairo-app-sdk/
  package.json            name: "kairo"  (import { View, Text, useState } from "kairo")
  src/jsx-runtime.ts      h(), Fragment  (jsxImportSource target)
  src/components.ts       View/Text/Pressable/ScrollView/Slider/Row/Col/... (tipe + tag)
  src/hooks.ts            useState/useEffect/useRef (minimal scheduler)
  src/system.d.ts         deklarasi global `kairo` (http/wifi/ble/storage/timer/log)
  src/manifest.ts         tipe KappManifest
  bin/kairo-build.ts      esbuild wrapper → bundle + manifest → app.kapp
  templates/counter/      contoh app (App.tsx, kapp.json)
```
- TSX → `h(type, props, ...children)`; `type` = string intrinsic ("View") atau
  fungsi (komponen). Output bundle: **satu IIFE/ESM**, target ES2020, minified,
  tree-shaken; tanpa Node API; global `kairo` dianggap ambient (disediakan host).
- `.kapp` format (v1): file teks `KAPP1\n<json-manifest-line>\n<js-bundle>`; atau
  cukup JS dengan header komentar manifest. (Container minimal, mudah di-parse C++.)

### 2.2 Engine layer (core)
```
firmware/core/include/kairo/js/
  js_engine.h        // wrap QuickJS: JSRuntime/JSContext, eval, callFn, GC, interrupt
  js_value.h         // RAII JSValue helper
  js_bridge.h        // node-desc(JS) → UiNode(NodeArena); handler table; reify()
  js_app.h           // JsApp : ComponentApp — build() = panggil komponen JS
  js_api.h           // daftar host-function (kairo.*) + capability gating
firmware/core/src/js/  (impl) + firmware/vendor/quickjs/  (vendored engine)
```
- `JsEngine`: satu `JSRuntime` (heap di PSRAM via custom malloc), satu `JSContext`
  per app. `setMemoryLimit`, `setInterruptHandler` (deadline → hentikan eval),
  `eval(bundle)`, `getDefaultExport()`, `callComponent()`.
- `JsBridge::reify(JSValue tree, NodeArena&)` → `UiNode*`. Map intrinsic string →
  `NodeType`; props → `Style` + text/role + handler id. Simpan callback JS di
  **handler table** (index → JSValue fn); `UiNode.onPress` = thunk C++ yang
  `engine.callHandler(id)`.
- `JsApp : ComponentApp`: `build()` = `bridge.reify(engine.callComponent(...))`.
  Karena turunan ComponentApp → otomatis dapat ComponentRuntime (scroll/gesture/
  focus/pause). Jalan di thread app (AppHost) → GC/JS tidak menghambat UI thread.

### 2.3 System API (`kairo.*`) — host functions
- Disuntik saat context dibuat, **hanya** yang lolos capability:
  `kairo.log(level,tag,msg)`, `kairo.device.{name,caps}`,
  `kairo.http.get/post(url,opts) → Promise`,
  `kairo.wifi.{scan,connect,status}`, `kairo.ble.{advertise,onData,…}`,
  `kairo.storage.{get,set,remove}` (per-app namespace di config store),
  `kairo.timer.{setTimeout,setInterval,clear}`.
- **Async**: kerja blocking (http/wifi/ble) dijalankan di **TaskRunner worker**;
  hasil me-resolve **Promise** di JS. Event loop QuickJS dipompa tiap iterasi
  app-loop (`JS_ExecutePendingJob`) + microtask drain. Tidak ada blocking di UI.

### 2.4 Storage & loader
- `IAppStore` (core): `list() → [KappMeta]`, `read(id) → bytes`, `install(bytes)`,
  `remove(id)`. Implementasi:
  1. `EmbeddedAppStore` — app bawaan dikompilasi ke firmware (demo).
  2. `FlashAppStore` — SPIFFS/NVS (persisten, "virtual storage" sekarang).
  3. `SdAppStore` — microSD (nanti, saat FS ada). Interface sama.
- `AppListScreen` gabung: native plugins + `IAppStore.list()` (JS apps). Pilih JS
  app → `AppHostManager.launch(jsApp)` (dapat pause/resume gratis).
- **Install over-the-wire**: channel KLP baru (`APP`/ext) — Forge kirim `.kapp` →
  device `IAppStore.install()` → muncul di Apps. (Reuse Plan 34/35 transport.)

### 2.5 Forge (DX di web, Plan 36)
- Halaman `/apps` (atau `/forge-apps`): editor/upload `.kapp`, daftar app
  ter-install di device, tombol **Install** (push via KLP), **Run**, **Remove**.
- (Opsional dev) **hot-load**: build TSX di browser (esbuild-wasm) → push tiap save.

---

## 3. Fase implementasi (tiap fase build & teruji)

| Fase | Isi | Tes |
|---|---|---|
| **0. SDK + build** | `packages/kairo-app-sdk`: `h()`, intrinsics, hooks, tipe, `kairo-build` (esbuild), template counter | bundle TSX→satu file; snapshot output; `bun run build` hijau |
| **1. Embed QuickJS** | vendor quickjs-ng; `JsEngine` (eval, callFn, mem-limit, interrupt); build **host+WASM+ESP-IDF** | `js_test` host: eval `1+1`, panggil fungsi, OOM/timeout ke-catch |
| **2. JS→UiNode bridge** | `JsBridge.reify` + handler table; `JsApp:ComponentApp`; render app JS statis | sim: app JS (View/Text/Pressable) tampil & ter-tap; node count benar |
| **3. Reaktivitas** | `useState/useEffect` + event-loop pump; re-render on setState | sim: counter JS interaktif (tap → angka naik), tanpa leak |
| **4. System API** | `kairo.log/http/wifi/ble/storage/timer`, capability-gated, async via TaskRunner+Promise | sim: app JS fetch http (wifi-gated) → tampil hasil; UI tak freeze |
| **5. Store + loader** | `IAppStore` (Embedded + Flash/SPIFFS); AppList gabung; launch via AppHostManager | device: app JS ter-embed muncul di Apps, jalan, bisa pause/resume |
| **6. Install OTA (KLP)** | channel APP; Forge push `.kapp` → install → muncul | Forge→device: install app baru tanpa reflash, langsung jalan |
| **7. microSD store** | `SdAppStore` saat FS siap | load app dari SD |
| **8. DX polish** | `create-kairo-app`, README, contoh (wifi/ble), hot-load Forge | dokumentasi + 2–3 contoh app |

Fase 0–4 **tanpa hardware** (host + WASM). 5–6 verifikasi device. 7 menyusul FS.

---

## 4. Contoh target (DX akhir)

```tsx
// App.tsx  (custom app)
import { View, Text, Pressable, useState } from "kairo";

export default function App() {
  const [n, setN] = useState(0);
  return (
    <View style={{ flexDirection: "column", padding: 4, gap: 6, alignItems: "center" }}>
      <Text variant="title">{`Count: ${n}`}</Text>
      <Pressable onPress={() => setN(n + 1)}><Text>+1</Text></Pressable>
      <Pressable onPress={async () => {
        const r = await kairo.http.get("https://api.example.com/x");
        kairo.log("info", "App", r.status);
      }}><Text>Fetch</Text></Pressable>
    </View>
  );
}
```
```jsonc
// kapp.json (manifest)
{ "id": "com.me.counter", "name": "Counter", "version": "1.0.0",
  "needs": ["http"], "entry": "App.tsx" }
```
```
kairo-build .        # → counter.kapp (manifest + minified JS)
# install: Forge → device (KLP), atau embed ke firmware
```

---

## 5. Acceptance criteria (definition of done)

- [ ] `kairo-build` mengubah TSX → satu `.kapp`; tipe TS jalan di editor.
- [ ] QuickJS ter-embed; build hijau host + WASM + skyrizz-e32.
- [ ] App JS render via `UiNode` yang sama (focus-ring, scroll, gesture, pause jalan).
- [ ] `onPress`/handler JS terpanggil dari tombol & touch.
- [ ] `useState` re-render; tidak ada memory leak (heap stabil setelah N render).
- [ ] System API capability-gated; app tanpa izin tak bisa akses; http async tak freeze UI.
- [ ] App JS muncul di Apps list (Embedded + Flash store), launch via AppHostManager.
- [ ] Install `.kapp` dari Forge via KLP → muncul & jalan tanpa reflash.
- [ ] Interrupt/mem-limit: app loop tak-henti / boros memori dihentikan, device tetap hidup.
- [ ] Contoh app (counter, http, wifi/ble) jalan di sim & device.

---

## 6. Memori & risiko

| Item | Catatan |
|---|---|
| QuickJS code | ~300–600KB flash (muat: skyrizz N16, ~70% free sekarang) |
| JS heap | di **PSRAM** (custom malloc) — per app context; mem-limit per app |
| GC pause | JS jalan di **thread app**, bukan UI → tak ganggu render |
| Runaway app | interrupt handler (deadline) + mem-limit → hentikan eval, app exit aman |
| Sandbox | hanya host-function yang di-expose; tak ada FS/net ambient |
| WASM | QuickJS C portable → ikut ke WASM (Forge sim jalankan app JS yang sama) |

## 7. Non-goals (v1)
- Bukan full React (no fiber/reconciler/concurrent) — functional + rebuild.
- Bukan npm-install-in-device — app = satu bundle mandiri (deps di-bundle saat build).
- Tidak ada multi-app berjalan paralel (tetap single-slot + pause, Plan 22).
- microSD = Fase 7 (butuh FS dulu).
