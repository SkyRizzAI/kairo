# 56 — App Runtime Architecture + Native (C Built-in) Tier

> Model runtime bersama: bagaimana 3 tier (C/WASM/JS) menancap ke ProcessContext +
> Surface + System API. Plus tier **C built-in** (di-compile bareng firmware).

- Status: ✅ Implemented — IAppRuntime interface + JsRuntime + CBuiltinRuntime adapters done; AppRegistry routes installCustom(IApp, AppManifest) with runtimeTier/displayServer; AppManifest extended with mode/category/iconPath/needs[]/apiVersion; WasmRuntime intentionally deferred to Plan 57 (wasm3 not vendored yet)
- Depends on: 48 (system api), 54 (process), 55 (surface)
- Blocks: 57 (WASM), 58 (JS)

---

## Goals

- Mendefinisikan **adapter pattern**: tiap runtime = binding tipis ke Host API
  yang sama (process, surface, system api).
- Tier **C built-in**: app native di-compile ke firmware, panggil host langsung,
  registrasi ke AppRegistry; juga rumah system apps tepercaya.
- Kontrak loader/lifecycle umum (launch, exit, kill, pause/resume) lintas tier.

## Keputusan

- Host API C++ runtime-agnostic dirancang **sebelum** runtime mana pun.
- C built-in = tier tercepat/tepercaya (tanpa marshalling); untuk system/first-party.
- WASM & JS dibangun di atas arsitektur ini (Plan 57/58).

---

## Latar belakang

Tiga keping fondasi sudah ada/terancang, tinggal **disatukan jadi satu kontrak
runtime** yang sama-sama dipakai C, WASM, dan JS:

**1. Eksekusi app sudah thread-per-proses (Plan 22).**

- `IApp` = `id()`, `name()`, `run(AppContext&)`, `fullscreen()`, `stackBytes()`
  (`core/include/nema/app/app.h:16-30`). App jalan di **thread sendiri**, boleh
  blok bebas tanpa membekukan UI.
- `AppHost` (`core/src/app/app_host.cpp`) menjahit `IApp` ke compositor: ia
  sekaligus `IScreen` (GUI thread) **dan** `AppContext` (app thread). Thread
  di-spawn di `enter()` (`app_host.cpp:79`: `thread_.start({"nema_app",
  app_.stackBytes(), 5, 0}, …)`), `run()` dipanggil di `threadEntry`
  (`app_host.cpp:159-162`), exit dideteksi di `tick()` lalu view di-`pop()`
  (`app_host.cpp:142-148`). Stack default **8 KB** (`app.h:29`), di-override per
  app (JS minta 256 KB — lihat Plan 58).
- `AppHostManager` menegakkan kebijakan **single-slot + pause/resume** Plan 22
  (`core/include/nema/app/app_host_manager.h:18-44`): satu foreground + satu
  paused; pause = thread park di `waitInput()` (`app_host.cpp:184-191`), CPU ~0,
  state utuh.

**2. Registry + manifest sudah punya konsep "tier".**

- `AppRegistry` membedakan `install()` (built-in, string literal) dari
  `installCustom()` (runtime-installed, JS `.kapp`) dan `launch(id)`
  (`core/include/nema/app/app_registry.h:33,37,62`).
- `AppManifest` (`core/include/nema/app/app_manifest.h:27-33`) sudah membawa
  `AppKind{BuiltIn,Custom}` + `AppType{App,Service}`. **Yang kurang: dimensi
  *runtime tier*** (C / WASM / JS) — `Custom` hari ini implisit = JS.

**3. Host API sedang dipecah jadi permukaan runtime-agnostic (Plan 54/55).**

- Plan 54 mengekstrak primitif **proses** dari `AppContext` jadi `ProcessContext`
  (argv/stdio/cwd/env/exit, `core/include/nema/proc/process_context.h`) +
  `ProcessHost` (thread+exit, ekstrak dari `app_host.cpp`).
- Plan 55 mengekstrak **surface** (canvas/present/input) jadi `ISurface` +
  `aether::createSurface(ctx, cfg)` (`core/include/nema/ui/surface.h`), diangkat
  **opsional** (proses default headless).
- Plan 48 = **System API** (interface `nema:<domain>/<name>`, mis. `nema:net/http`;
  host contract `HostApi` pure-virtual, Plan 49 — device/storage/http/wifi/…),
  capability-gated. Plan 50/52 = **UI SDK** `aether:ui` (node-tree).

**Celah yang ditutup Plan 56:** keempat permukaan itu (proses, surface, system
API, UI SDK) hari ini cuma punya **satu** konsumen — app C++ yang memanggilnya
langsung. Plan 56 menamai gabungan itu sebagai **Host API** dan mendefinisikan
*bagaimana sebuah runtime tier menancap ke sana*, sehingga WASM (Plan 57) dan JS
(Plan 58) cukup jadi **adapter tipis**, bukan menulis ulang lifecycle. Tier
pertama yang dibuktikan = **C built-in** (adapter identitas: app *adalah* C++,
memanggil Host API tanpa marshalling).

### Pelajaran dari referensi

| Aspek | Flipper Momentum | AkiraOS (Zephyr) | Keputusan Palanu |
|---|---|---|---|
| Tier app | Native ELF `.fap` (`lib/flipper_application/elf/`) **dan** mJS — dua jalur terpisah, API beda | Satu tier: WASM via WAMR (`src/runtime/wamr_config.h`) | **Tiga tier (C/WASM/JS) di atas SATU Host API** — adapter tipis, bukan jalur paralel |
| Resolusi simbol | Hash-table API per-firmware (`api_hashtable/`) → ELF relocate | Native module register ke WAMR (`akira_native_api.h:341`) | Host API C++ = link-time (C built-in) / import table (WASM) / binding (JS) |
| Lifecycle | `FuriThread` per-app, loader callback `void` (tak ada state) | State machine `APP_STATE_NEW→INSTALLED→RUNNING→STOPPED/ERROR/FAILED` (`app_manager.h:72-80`) + event cb | **Satu lifecycle (ProcessHost+WindowPolicy) dipakai semua tier**; tambah state `Killed` (watchdog) |
| Tepercaya vs sandbox | ELF = **tak ada sandbox** (native, akses penuh) | Semua app WASM = sandboxed seragam | **Tier-aware**: C built-in tepercaya (first-party), WASM/JS sandboxed (Plan 57/58) |
| Registry | `.fap` di SD + manifest (`application_manifest.h`) | `app_manager` install/state table | `AppRegistry` + `AppManifest` (sudah ada) ditambah dimensi `runtime` |
| Introspeksi | — | `app_manager_get_state()`, `app_state_to_str()` | `ps` (Plan 46) baca state per-proses lintas tier |

Flipper membuktikan "dua tier = dua API" itu mahal (mJS dan ELF nyaris tak berbagi
apa pun). AkiraOS membuktikan **satu lifecycle/state-machine** untuk semua app
berharga untuk introspeksi, tapi ia cuma punya **satu** bahasa (WASM). Palanu
mengambil keduanya: **satu lifecycle** (AkiraOS) tapi **tiga tier** di atas **satu
Host API** (mengalahkan dua-jalur Flipper), dengan tier C built-in tepercaya untuk
system apps (yang di Flipper = ELF tanpa sandbox, di Akira = tak ada).

---

## Desain teknis

### 1. Host API = empat permukaan runtime-agnostic

"Host API" bukan satu kelas baru — ia **kontrak gabungan** yang sudah/akan ada,
dirancang sebelum runtime mana pun. Sebuah app (tier apa pun) hanya boleh
menyentuh ini:

```
┌──────────────────────── Host API (C++, runtime-agnostic) ────────────────────┐
│  ProcessContext   argv / stdin·stdout·stderr / cwd·env / exit(code)  (Plan 54)│
│  ISurface         createSurface() → canvas / submit / input         (Plan 55)│
│  System API       device / storage / http / wifi / ble / timer …    (Plan 48)│
│  aether:ui        node-tree komponen (View/Text/Pressable/…)     (Plan 50/52)│
└──────────────────────────────────────────────────────────────────────────────┘
            ▲                    ▲                     ▲
   identitas (link)      import table (wasm3)   binding (QuickJS)
            │                    │                     │
      C built-in              WASM                    JS
       (Plan 56)            (Plan 57)              (Plan 58)
```

Inti adapter pattern: **lifecycle, threading, surface, dan kebijakan window TIDAK
diduplikasi per tier**. Mereka hidup sekali di `ProcessHost` (Plan 54) +
`Surface`/`IWindowPolicy` (Plan 55). Sebuah tier hanya menyumbang **satu hal**:
cara *entry point*-nya dipanggil dan cara *Host API* dijembatani ke model
eksekusinya (panggilan langsung / import wasm / C-function JS).

### 2. `IAppRuntime` — kontrak loader per tier

```cpp
// core/include/nema/app/app_runtime.h
namespace nema {

enum class RuntimeTier { CBuiltin, Wasm, Js };

// Sebuah tier mengubah "paket app" (in-memory bytes / pointer C++) menjadi
// sebuah IApp yang run()-nya mengeksekusi app di model runtime tier itu.
// Lifecycle (thread/exit/pause/surface) di luar scope ini — itu milik ProcessHost.
struct IAppRuntime {
    virtual ~IAppRuntime() = default;
    virtual RuntimeTier tier() const = 0;

    // Bisa-kah tier ini memuat paket ini? (cek magic: ELF-link vs 0x00 'asm' vs JS)
    virtual bool canLoad(const AppManifest& m) const = 0;

    // Instansiasi app. Untuk C built-in = kembalikan IApp yang sudah ada apa adanya;
    // untuk WASM/JS = bungkus engine dalam IApp (WasmApp/JsApp) yang run()-nya
    // menjalankan interpreter + memompa Host API.
    virtual IApp* instantiate(Runtime& rt, const AppManifest& m,
                              const uint8_t* pkg, size_t len) = 0;
    virtual void destroy(IApp* app) = 0;
};

} // namespace nema
```

- **C built-in = adapter identitas.** `CBuiltinRuntime::instantiate` mengembalikan
  pointer ke `IApp` yang **sudah** di-`install()` ke registry — tanpa parsing,
  tanpa marshalling, tanpa engine. Inilah kenapa ia tier tercepat & tepercaya.
- **WASM/JS** (Plan 57/58) mengembalikan `WasmApp`/`JsApp` (turunan `IApp`) yang
  `run(ProcessContext&)`-nya = instantiate interpreter + loop yang memompa Host
  API. Dari sudut `ProcessHost`, ketiganya **identik** — cuma `IApp`.

`RuntimeRegistry` (kecil, dimiliki `Runtime`) memetakan `RuntimeTier` →
`IAppRuntime`. `AppRegistry::launch(id)` melihat `manifest.runtime`, pilih
runtime, `instantiate()`, lalu serahkan `IApp` ke `AppHostManager`/`ProcessHost`
yang **tak peduli tier**.

### 3. `AppManifest` ditambah dimensi runtime

```cpp
struct AppManifest {
    const char* id;       const char* name;     const char* version;
    AppKind     kind;     // BuiltIn | Custom (sudah ada — sumber: firmware vs OTA)
    AppType     type;     // App | Service       (sudah ada)
    RuntimeTier runtime;  // BARU: CBuiltin | Wasm | Js (cara dieksekusi)
};
```

> Plan 56 hanya menambah **dimensi `runtime`** (`RuntimeTier`) ke `AppManifest`.
> Skema manifest **lengkap** (`display_server`, `mode`, `needs[]`, `api_version`,
> `category`, `icon`, …) + reuse enum `RuntimeTier` ini = **Plan 59**.

`kind` (asal) dan `runtime` (eksekutor) **ortogonal**: built-in bisa C *atau* JS
(JS app yang di-embed ke firmware, Plan 37 Fase 5); custom OTA bisa WASM *atau*
JS. `install()` → `runtime=CBuiltin`; `installCustom()` mendeteksi dari magic
paket (`\0asm` → Wasm, selainnya → Js) atau dari field manifest `.kapp`/`.wasm`.

### 4. Lifecycle umum (launch/exit/kill/pause-resume) — satu jalur

| Fase | Mekanisme (dipakai SEMUA tier) | Sumber |
|---|---|---|
| **launch** | `AppRegistry::launch` → pilih `IAppRuntime` → `instantiate()` → `AppHostManager::launch(IApp&)` → `ProcessHost::start()` spawn thread | `app_registry.h:62`, `app_host.cpp:79` |
| **run** | `thread.run` → `app.run(ProcessContext&)`; UI opsional via `createSurface` | `app_host.cpp:159-162` (Plan 54/55) |
| **exit** | app `requestExit(code)` / `run()` return → `ProcessHost::join` ambil exit code → `pop()` | `app_host.cpp:142-148,195` (Plan 54) |
| **pause/resume** | thread park di `waitInput()` (paused flag, CPU ~0) / `enter()` clear flag | `app_host.cpp:184-191` (Plan 22) |
| **kill** | `requestExit` + **watchdog**: jika thread tak balik dalam grace window → paksa-hentikan | BARU (lihat §5) |

**Perbedaan tier ada di SATU titik saja: `kill` paksa.** C built-in tepercaya →
`requestExit` cukup (kontrak: app C++ wajib hormati `shouldExit()`). WASM/JS tak
tepercaya → butuh **terminasi engine** (wasm3: hancurkan runtime / trap; QuickJS:
interrupt handler + heap dispose) karena app jahat bisa abaikan `shouldExit()`.

### 5. Kill / watchdog — kontrak tier-aware

```cpp
// ProcessHost menambah state + watchdog:
enum class ProcState { New, Running, Exited, Killed };

// kill(reason): coba graceful, lalu paksa sesuai tier.
void ProcessHost::kill(KillReason r) {
    requestExit(/*code*/ 137);                 // 1) graceful: set shouldExit()
    if (joinFor(graceMs_)) return;             // 2) tunggu grace window
    runtime_->forceTerminate(app_, *this);     // 3) paksa via IAppRuntime tier
}
```

`IAppRuntime::forceTerminate` = no-op untuk C built-in (tak ada cara aman
mem-preempt thread C++ → kita *percaya* first-party); untuk WASM = set trap +
delete `IM3Runtime` (Plan 57); untuk JS = arm interrupt handler agar `JS_Eval`
balik + dispose `JSRuntime` (Plan 58). State `Killed` muncul di `ps` (Plan 46).

### 6. Tier C built-in — detail

- **Registrasi**: app C++ `install()`/`installService()` ke `AppRegistry`
  (`app_registry.h:33,51`), persis sekarang. `CBuiltinRuntime` membungkusnya jadi
  `IAppRuntime` agar seragam dengan WASM/JS, tapi `instantiate()` cuma return
  pointer yang sudah ada (umur = firmware).
- **Tanpa marshalling**: app memanggil `ctx.out().writeStr(...)`,
  `aether::createSurface(...)`, dan System API generated `nema_net_http_get(...)`
  (header `<nema.h>`, Plan 49/50) **langsung** sebagai pemanggilan C++ — nol copy
  buffer, nol translasi pointer linear-memory. Parity-by-construction `HostApi`
  (kelas abstrak pure-virtual, Plan 49) berlaku **gratis** untuk tier ini karena
  app memanggil Host API yang sama tanpa lapisan adapter.
- **Tepercaya**: tier ini rumah **system/first-party apps** (settings, launcher,
  shell tools). Tak ada capability gating runtime — first-party = bagian firmware.
  (Sandbox/quota dimulai di WASM/JS.)
- **Service**: `installService` (`app_registry.h:51`) tetap jalur C built-in
  (daemon background), tak pernah WASM/JS di v1.

---

## Fase

- [ ] **Fase 1 — Kontrak Host API + `IAppRuntime`/`RuntimeTier`.** Definisikan
      `app_runtime.h` (`IAppRuntime`, `RuntimeTier`, `RuntimeRegistry`); tambah
      `runtime` ke `AppManifest`. Belum ubah eksekusi. Host test: registry pilih
      tier dari manifest.
- [ ] **Fase 2 — `CBuiltinRuntime` (adapter identitas).** Bungkus jalur
      `install()` yang ada jadi `IAppRuntime`; `AppRegistry::launch` lewat
      `RuntimeRegistry`. **Nol perubahan perilaku** app C++ sekarang. Parity test
      host+WASM: semua app built-in jalan identik.
- [ ] **Fase 3 — Lifecycle umum di atas ProcessHost (sinkron Plan 54/55).**
      `ProcessHost` + `Surface` memegang thread/exit/surface; `AppHostManager`
      jadi `SingleForegroundPolicy`. C built-in jalan murni di atas Host API
      (`run(ProcessContext&)` + `createSurface`). Test: launch/exit/pause/resume +
      exit code lintas C apps.
- [ ] **Fase 4 — Kill + watchdog + state `Killed`.** `ProcessHost::kill` graceful→
      paksa via `forceTerminate` (no-op untuk C). `ps` tampilkan state. Test: app
      C++ yang hormati `shouldExit()` mati bersih; stub force-terminate siap untuk
      Plan 57/58.

**Build/uji:** host + WASM tiap fase; ESP32 build-only Fase 2 & 4.

---

## File yang disentuh

- **Baru:** `core/include/nema/app/app_runtime.h` (`IAppRuntime`, `RuntimeTier`,
  `RuntimeRegistry`), `core/src/app/cbuiltin_runtime.cpp` (adapter identitas),
  `core/src/app/runtime_registry.cpp`.
- **Diubah:** `core/include/nema/app/app_manifest.h` (field `runtime`),
  `core/include/nema/app/app_registry.{h,cpp}` (`launch` lewat `RuntimeRegistry`,
  set `runtime` di `install`/`installCustom`), `core/include/nema/app/
  app_host_manager.h` + `core/src/app/app_host.cpp` (`kill`/`forceTerminate`,
  state `Killed`; sejalan ekstraksi `ProcessHost` Plan 54 / `Surface` Plan 55),
  `core/{include,src}/nema/runtime.*` (`runtimes()` accessor), `46-process-
  monitor` `ps` (kolom RUNTIME + state).
- **Konsumen:** Plan 57 daftarkan `WasmRuntime`, Plan 58 daftarkan `JsRuntime` ke
  `RuntimeRegistry` — keduanya hanya implementasi `IAppRuntime`.

---

## Test

- **Unit (host):** `RuntimeRegistry` memetakan manifest→tier; `canLoad` magic
  (ELF-link/`\0asm`/JS) benar; `instantiate`/`destroy` C built-in nol-alokasi.
- **Parity (host+WASM):** seluruh app built-in jalan identik setelah dibungkus
  `CBuiltinRuntime` (launch, exit code, pause/resume Plan 22 tak berubah).
- **Lifecycle:** launch→run→exit (exit code), pause→resume (state utuh), kill
  graceful (app C++ yang hormati `shouldExit()` mati < grace window).
- **Introspeksi:** `ps` (Plan 46) tampilkan kolom RUNTIME + state lintas tier
  (stub WASM/JS belum ada → hanya C).
- **ESP32:** build-only — pastikan tabel runtime + manifest tak menambah footprint
  berarti.

---

## Risiko & mitigasi

- **Over-abstraksi (3 tier, MCU).** → Adapter pattern justru *mengurangi* kode:
  lifecycle ditulis sekali (`ProcessHost`), tier cuma entry+jembatan. C built-in =
  adapter identitas (nol overhead). Tak ada VM-of-VM.
- **Regresi saat membungkus app C++ ke `CBuiltinRuntime`.** → Adapter identitas
  return pointer apa adanya; parity test host+WASM jadi gate tiap fase.
- **`kill` paksa tak aman untuk C++.** → Justru itu: C built-in **tepercaya**,
  `forceTerminate` = no-op (kita percaya first-party hormati `shouldExit()`).
  Preemption paksa hanya untuk WASM/JS yang punya titik terminasi aman (trap /
  interrupt) — lihat Plan 57/58.
- **Manifest dua dimensi (`kind`×`runtime`) membingungkan.** → Dokumentasikan:
  `kind` = *asal* (firmware/OTA), `runtime` = *eksekutor* (C/WASM/JS); ortogonal.
- **Coupling ke Plan 54/55 yang belum jadi.** → Fase 1–2 bisa jalan di atas
  `AppContext` lama; Fase 3 menukar ke `ProcessContext`/`ISurface` saat 54/55
  mendarat. Urutan dijaga (56 blocks 57/58, bukan 54/55).

---

## Yang sengaja TIDAK dikerjakan (sekarang)

- **Hot-swap runtime / multi-bahasa per app** — satu app = satu tier.
- **Sandbox/quota untuk C built-in** — first-party tepercaya; sandbox dimulai di
  WASM/JS (Plan 57/58).
- **Multi-app paralel** — tetap single-slot + pause (Plan 22/55).
- **Tier ELF native loadable (ala Flipper `.fap`)** — non-portabel per-arch;
  WASM menggantikan peran "native loadable" dengan portabilitas (Plan 57).
