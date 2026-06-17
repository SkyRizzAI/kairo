# 57 — App Runtime: WASM Tier

> Runtime app portabel & sandboxed: C/C++/Rust/Zig → wasm32, dijalankan oleh
> **wasm3** yang di-compile ke dalam core (satu jalur di ESP32 & simulator).

- Status: 🟢 Implemented (Fase 1–3) — wasm3 vendored (`vendor/wasm3/`), `WasmEngine` wrapper (init/load/runStart), WASI bridge (`wasm_wasi.cpp` — fd_read/write, args_get, proc_exit) delegating to `ProcessContext`, `WasmRuntime : IAppRuntime` adapter; Fase 4–6 deferred (SysAPI gating, aether:ui surface import, memory quota — need test .wasm assembly)
- Depends on: 49 (SDK gen), 54 (process/WASI), 55 (surface), 56 (arch)
- Blocks: 59 (manifest/packaging)

---

## Goals

- Integrasi wasm3 (interpreter) ke Nema core; satu jalur eksekusi device + sim.
- **WASI** untuk stdio/argv (Plan 54); **imports** untuk System API (Plan 48) +
  surface/UI (Plan 55).
- Sandbox + capability gating per interface (import hanya yang diizinkan).
- Loader `.wasm`, memory quota per-app, watchdog/timeout.

## Keputusan

- **Ship `.wasm`, interpret** (wasm3). **AOT ditunda** (per-arch → non-portabel).
- **Jangan** optimasi simulator pakai engine browser native → cegah dua-jalur/divergensi.
- Host API diakses via imports (= adapter WASM dari Plan 56).

---

## Latar belakang

Tier WASM adalah **adapter ketiga** di atas Host API Plan 56 — yang pertama yang
benar-benar *sandboxed* (C built-in tepercaya). Fondasinya sudah berdiri:

- **Adapter contract (Plan 56)**: `IAppRuntime`/`RuntimeTier::Wasm`; tier ini cukup
  mengembalikan `WasmApp : IApp` yang `run(ProcessContext&)`-nya = instantiate
  wasm3 + loop. Lifecycle (thread/exit/pause/kill/surface) **sudah** dipegang
  `ProcessHost`+`Surface`+`SingleForegroundPolicy` — tier tak menulis ulang apa
  pun (`56-app-runtime-architecture-native.md`).
- **WASI mapping (Plan 54)**: Plan 54 §6 sudah memetakan 5 import WASI
  (`args_get`/`args_sizes_get`, `fd_read`, `fd_write`, `proc_exit`) ke
  `ProcessContext` (`54-process-model-shell-execution.md:271-287`). Tier WASM
  tinggal **menyediakan implementasi import** itu.
- **Surface (Plan 55)**: `aether::createSurface(ctx, cfg)` →
  `ISurface{canvas/submit/nextInput}` (`core/include/nema/ui/surface.h`). Untuk
  WASM ini menjadi **import interface `aether:ui`** (Plan 55 §2:
  "WASM import interface `aether:ui`", `55-surface-window-model.md:153-156`).
- **Build dual-target sudah terbukti** (lihat Plan 37 untuk QuickJS): pola vendor
  C portable di `firmware/vendor/<lib>/CMakeLists.txt` dengan cabang
  `if(ESP_PLATFORM) idf_component_register(...) else() add_library(...) endif()`
  (`firmware/vendor/quickjs/CMakeLists.txt`), di-`add_subdirectory` dari
  `firmware/CMakeLists.txt`, di-link ke `nema_core`. **wasm3 ikut pola ini.**

**Yang penting & non-intuitif — satu jalur, dua target:** simulator Palanu
**bukan** native lagi; ia di-compile ke **wasm32 via Emscripten**
(`firmware/targets/wasm/`, `nema.js`+`nema.wasm`, `-sINITIAL_MEMORY=512MB`). Maka
saat app WASM jalan di simulator, kita menjalankan **interpreter wasm3 (yang
sendiri sudah jadi wasm32) untuk meng-interpret guest `.wasm`** — "wasm di dalam
wasm". Itu **disengaja**: satu codepath (wasm3) di device **dan** sim. Godaan
"optimasi sim pakai `WebAssembly.instantiate` browser native" **ditolak** — itu
bikin dua jalur eksekusi yang divergen (semantik trap, memory, import beda) dan
membunuh nilai sim sebagai *test ground* device. **wasm3 interpret di mana-mana.**

**Celah yang ditutup Plan 57:** vendor wasm3, tulis loader `.wasm`, sediakan
implementasi import (WASI + System API `nema:<domain>/<name>` + `aether:ui`) yang men-*delegasi* ke Host
API, dan pasang tiga pagar sandbox (capability gating per-interface, memory quota,
watchdog) — agar app dari pihak ketiga (Rust/Zig/C) bisa di-load tanpa
mempercayainya.

### Pelajaran dari referensi

| Aspek | Flipper Momentum | AkiraOS (Zephyr) | Keputusan Palanu |
|---|---|---|---|
| Engine | **Tak ada WASM** — native ELF `.fap` (`lib/flipper_application/elf/`) | **WAMR** (`src/runtime/wamr_config.h:12`) | **wasm3** (interpreter murni, ~64 KB code, C portable) |
| Portabilitas | ELF per-arch (ARM only), relokasi simbol | WASM portabel ✅ | WASM portabel ✅ — **ship `.wasm`, interpret** (AOT ditunda) |
| Sandbox | **Tak ada** — ELF akses penuh | Linear memory + native API gated | **Sandbox**: linear memory bounded + import gated per-interface |
| Memory model | Heap furi global | Heap 512 KB + **64 KB/instance** + stack 256 KB (`wamr_config.h:23-30`) | **Quota per-app**: cap linear-memory pages + stack dari `stackBytes()` |
| stdio/argv | CLI satu arah | Native module, tak ada WASI | **WASI** (`fd_read/fd_write/args_get/proc_exit`) → `ProcessContext` (Plan 54) |
| Lifecycle | Loader callback `void` | `app_manager` state + event `WAMR_EVENT_*` (`wamr_config.h:109-113`) | `ProcessHost` state (Plan 56) — satu lifecycle lintas tier |
| Eksekusi sim | — | QEMU/native | **wasm3-in-wasm32** (sim = device codepath, BUKAN engine browser) |

AkiraOS adalah cetak biru terdekat (WASM sandbox di MCU) tapi memilih **WAMR** —
lebih besar, dan ia mengikat ke Zephyr. Palanu memilih **wasm3**: jauh lebih kecil
(penting karena kita juga membawa QuickJS, Plan 58 — dua engine harus muat),
interpreter murni (tak ada AOT per-arch yang merusak portabilitas), dan C portable
sehingga **identik di ESP32 dan di simulator wasm32**. Footprint per-instance Akira
(64 KB instance heap, 256 KB stack) jadi acuan angka quota kita. Flipper = pelajaran
negatif: ELF native = nol sandbox + per-arch — persis dua hal yang WASM hindari.

---

## Desain teknis

### 1. Vendor wasm3 (dual-build)

`firmware/vendor/wasm3/` mengikuti pola `quickjs`:

```cmake
# firmware/vendor/wasm3/CMakeLists.txt  (MIT)
set(WASM3_SRCS source/m3_*.c source/wasm3.c)        # interpreter murni, C99
if(ESP_PLATFORM)
    idf_component_register(SRCS ${WASM3_SRCS} INCLUDE_DIRS "source")
    target_compile_options(${COMPONENT_LIB} PRIVATE -w -Dd_m3HasWASI=0)
else()                                               # host + WASM(emscripten)
    add_library(wasm3 STATIC ${WASM3_SRCS})
    target_include_directories(wasm3 PUBLIC source)
    target_compile_options(wasm3 PRIVATE -w)
endif()
```

`add_subdirectory(vendor/wasm3)` di `firmware/CMakeLists.txt`; link
`target_link_libraries(nema_core … wasm3)` (host) / `REQUIRES wasm3` (IDF). Kita
**matikan WASI bawaan wasm3** (`d_m3HasWASI=0`) dan implementasi WASI sendiri agar
nyambung ke `ProcessContext` (Plan 54), bukan ke stdout host.

### 2. `WasmEngine` — wrapper wasm3

```cpp
// core/include/nema/wasm/wasm_engine.h
class WasmEngine {
public:
    bool init(size_t stackBytes, size_t memQuotaBytes);   // m3_NewEnvironment + m3_NewRuntime
    bool load(const uint8_t* wasm, size_t len);           // m3_ParseModule + m3_LoadModule
    bool linkHost(ProcessContext& ctx, const CapSet& caps); // §3 — link import yg diizinkan
    int  runStart(/*argv via WASI*/);                     // m3_FindFunction("_start") + m3_CallV
    void pumpDeadline();                                  // watchdog poke (§6)
    void terminate();                                     // trap + m3_FreeRuntime (kill, Plan 56)
private:
    IM3Environment env_{}; IM3Runtime rt_{}; IM3Module mod_{};
};
```

Satu `IM3Runtime` per app (analog satu `JSRuntime`/app di JS). `m3_NewRuntime(env,
stackBytes, userdata)` — **stack interpreter** diambil dari `app.stackBytes()`
(`app.h:29`); userdata = `&WasmApp` agar trampolin import bisa balik ke
`ProcessContext`.

`WasmApp : IApp` (didaftarkan ke `RuntimeRegistry` sebagai `WasmRuntime`,
Plan 56): `run(ProcessContext& ctx)` = `init → load → linkHost → runStart`;
return value `_start`/`proc_exit` → exit code.

### 3. Import: WASI + System API (`nema:<domain>/<name>`) + `aether:ui` (capability-gated)

Semua interaksi guest↔host lewat **import functions**. wasm3 `m3_LinkRawFunction
(module, "<modName>", "<fn>", "<sig>", &trampoline)`. Tiga kelompok modul:

**a. WASI (`wasi_snapshot_preview1`)** — stdio/argv/exit, gratis untuk toolchain
Rust/C/Zig (mereka "merasa" POSIX). Set minimal:

| Import WASI | Delegasi ke (Plan 54) |
|---|---|
| `args_sizes_get`, `args_get` | `ctx.args()` |
| `environ_sizes_get`, `environ_get` | `ctx.env()` |
| `fd_read(0,…)` | `ctx.in().read()` |
| `fd_write(1/2,…)` | `ctx.out()/err().write()` |
| `fd_close`, `fd_seek`, `fd_fdstat_get` | stub minimal (fd 0/1/2 saja) |
| `proc_exit(code)` | `ctx.requestExit(code)` → trap balik dari `_start` |
| `clock_time_get`, `random_get` | `rt.clock()`, RNG core |

**b. System API (Plan 48)** — modul import **per-interface** dengan id kanonik
`nema:<domain>/<name>` (mis. `nema:net/http`, `nema:storage/kv`, `nema:net/wifi`,
`nema:bt/ble`, `nema:sys/tasks`; modul WASM = interface id, skema Plan 48 §2).
Catatan: `nema:sys/*` hanya domain core (log/device/events/tasks/power) — **bukan**
payung seluruh System API. Satu import per fungsi, delegasi ke Host API impl
(`NemaHostImpl : HostApi`, Plan 49).

**c. `aether:ui`** — surface + node-tree Plan 55/50: `surface_create`,
`surface_submit`, `surface_next_input`, `node_*` (bangun pohon `UiNode` dari sisi
guest). `surface_create` memanggil `aether::createSurface(ctx, cfg)`; `nullptr` →
import balik error (app headless tetap jalan, Plan 55).

**Capability gating per-interface (fail-closed):** `linkHost(ctx, caps)` **hanya
me-link import yang di-grant** (manifest `needs[]` ∩ kapabilitas board, Plan 48/59).
Import yang tak di-grant **tidak di-link** → saat guest memanggilnya, wasm3
menghasilkan *unresolved import* di `LoadModule` → **load gagal cepat** (lebih
aman daripada link stub yang return error: app jahat tak bisa menyentuh interface
yang tak ia minta). WASI (stdio/exit) selalu di-link (bukan privilege).

### 4. Marshalling & batas memori linear

Pointer dari guest = **offset ke linear memory**, bukan alamat host. Tiap
trampolin import:

```cpp
m3ApiRawFunction(wasi_fd_write) {
    m3ApiGetArg(int32_t, fd); m3ApiGetArgMem(const iovec*, iov); …
    uint8_t* base; uint32_t memSize = 0;
    base = m3_GetMemory(runtime, &memSize, 0);
    // WAJIB: validasi [offset, offset+len) ⊂ [0, memSize) sebelum deref — sandbox.
    if (!boundsOk(iov_off, iov_len, memSize)) m3ApiTrap("oob");
    ctx->out().write(base + iov_off, iov_len);
    m3ApiReturn(0);
}
```

**Bounds-check setiap akses memori guest = inti sandbox.** wasm3 sudah bounds-check
instruksi `load/store` guest; yang harus *kita* jaga adalah saat **host** membaca
buffer yang guest tunjuk (iovec, string, struct) — satu helper `boundsOk()` dipakai
semua trampolin.

### 5. Memory quota per-app

- Linear memory wasm3 tumbuh via `memory.grow`. **Quota** = batasi `maxPages`
  (`d_m3MaxLinearMemoryPages` per-runtime / cek di trampolin `memory.grow`): app
  yang minta tumbuh melewati quota → `grow` return -1 (guest lihat OOM, bukan
  crash host). Default acuan: ikut Akira **64 KB/instance** (1 page = 64 KB),
  override per-manifest.
- Stack interpreter = `app.stackBytes()` (default 8 KB; app WASM berat override).
  Di ESP32, alokasi stack besar → PSRAM (jalur `thread_esp32.cpp` yang sama
  dipakai JS).

### 6. Watchdog / timeout — runaway tanpa preempt

wasm3 **tak punya interrupt handler** (beda dari QuickJS). Loop guest `while(1){}`
tanpa host-call **tak bisa di-preempt dari dalam**. Mitigasi dua lapis:

1. **Deadline via host-call budget (soft):** tiap trampolin import poke
   `pumpDeadline()`; jika `now − turnStart > deadlineMs` → `m3ApiTrap` → `_start`
   balik dengan error → exit. Menangkap app yang *sering* memanggil host.
2. **Watchdog thread (hard):** app WASM jalan di thread `ProcessHost` sendiri.
   `ProcessHost::kill` (Plan 56 §5) untuk WASM = `WasmEngine::terminate()`: set
   flag trap + `m3_FreeRuntime` dari thread watchdog → thread guest dibongkar.
   Menangkap loop ketat tanpa host-call. (Inilah `forceTerminate` Plan 56 untuk
   tier WASM.)

Karena tiap app punya thread + runtime terisolasi, membongkar satu app tak
menyentuh yang lain (beda dari single-context).

### 7. Loader `.wasm`

`WasmRuntime::canLoad` = magic `\0asm` (`00 61 73 6D`). `installCustom`
(`app_registry.h:37`) men-deteksi magic → `runtime=Wasm`. Paket `.wasm` (atau
container `.papp`/PAPP1 Plan 59 yang membungkus `.wasm` + manifest) → `WasmEngine::load`.
Manifest (Plan 59) membawa `needs[]` (capability) + quota override + `display_server`
(binding `aether:ui`, Plan 55).

---

## Fase

- [ ] **Fase 1 — Vendor wasm3 + `WasmEngine` (eval).** `firmware/vendor/wasm3` dual-
      build; wrapper `init/load/runStart`; jalankan `.wasm` "add(2,3)" tanpa import.
      Build host + WASM(emscripten) + ESP-IDF. Host test: load+call, trap ke-catch.
- [ ] **Fase 2 — WASI stdio/argv/exit (sinkron Plan 54 Fase 4).** Link 5+ import
      WASI ke `ProcessContext`; `_start`; exit code. Smoke: app Rust/C `cat`/`echo
      $argv` baca stdin→stdout, exit code benar. Bounds-check + helper `boundsOk`.
- [ ] **Fase 3 — `WasmRuntime` + loader (sinkron Plan 56).** Daftar ke
      `RuntimeRegistry`; `canLoad` magic; `WasmApp:IApp`; launch dari shell `run
      app.wasm`. Test: app WASM headless muncul di `ps`, launch/exit lintas tier.
- [ ] **Fase 4 — System API (`nema:<domain>/<name>`) + capability gating per-interface.**
      Link interface System API (Plan 48) hanya yang di-grant; ungranted → load gagal
      cepat. Test: app minta `net.http`
      tanpa grant → load ditolak; dengan grant → fetch jalan (off UI thread).
- [ ] **Fase 5 — `aether:ui` surface + node-tree (sinkron Plan 55).** Import
      `surface_*`/`node_*`; app WASM gambar UI via `createSurface`. Sim: app Rust
      render View/Text/Pressable, ter-stream ke Forge (`RemoteScreenTap`).
- [ ] **Fase 6 — Memory quota + watchdog.** Cap `maxPages`; deadline host-call +
      watchdog-thread `terminate()`. Test: app `while(1)` tanpa host-call → di-kill,
      OS hidup; app minta memori > quota → OOM bersih (guest), host utuh.

**Build/uji:** host + WASM tiap fase; ESP32 build-only Fase 1, 3, 6.

---

## File yang disentuh

- **Baru:** `firmware/vendor/wasm3/` (+ `CMakeLists.txt`),
  `core/include/nema/wasm/wasm_engine.h` + `core/src/wasm/wasm_engine.cpp`,
  `core/src/wasm/wasm_wasi.cpp` (import WASI → `ProcessContext`),
  `core/src/wasm/wasm_imports_sys.cpp` (System API `nema:<domain>/<name>`), `core/src/wasm/
  wasm_imports_ui.cpp` (`aether:ui`), `core/src/wasm/wasm_app.cpp` (`WasmApp:IApp`
  + `WasmRuntime:IAppRuntime`).
- **Diubah:** `firmware/CMakeLists.txt` (`add_subdirectory(vendor/wasm3)`),
  `core/CMakeLists.txt` (link wasm3), `app_registry.cpp` (`installCustom` deteksi
  magic `\0asm`), `runtime.cpp` (daftar `WasmRuntime` ke `RuntimeRegistry`).
- **SDK (Plan 49):** target `wasm32-wasi` + header import `nema:<domain>/<name>`/`aether:ui`
  untuk C/Rust/Zig; contoh `cat.wasm` + UI app.

---

## Test

- **Unit (host):** load `.wasm`, call export, trap; `boundsOk` tolak OOB iovec;
  quota `memory.grow` > maxPages → -1.
- **WASI (host+WASM):** `echo argv`, `cat` (stdin→stdout), exit code; pipe `a.wasm
  | b.wasm` (Plan 54) lintas-tier.
- **Capability:** app tanpa grant `http` → load gagal cepat (import unresolved);
  dengan grant → fetch off-UI; interface tak diminta tak terjangkau.
- **UI (WASM sim):** app Rust render node-tree, ter-stream Forge; geometri dari
  `surface.width()/height()` (resolution-independent).
- **Watchdog (sim+device):** `while(1)` tanpa host-call → `terminate()` membongkar
  app, app lain & OS hidup; deadline host-call trap untuk loop yang sering syscall.
- **ESP32:** build-only — footprint wasm3 + stack PSRAM waras.

---

## Risiko & mitigasi

- **Dua engine (wasm3 + QuickJS) di flash ESP32.** → wasm3 ~64 KB code (mungil);
  QuickJS ~300–600 KB (Plan 58/37). Skyrizz N16 masih ~60% free dengan QuickJS
  saja (Plan 37) → wasm3 muat lapang. Heap guest di PSRAM. Ukur per fase
  (ESP32 build-only gate).
- **Interpreter lambat (wasm3) di ESP32.** → Diterima: app WASM = portabilitas >
  kecepatan puncak; hot path tetap C built-in. AOT **ditunda** (per-arch →
  non-portabel, melawan satu-jalur). Ukur app nyata; jika perlu, *cache* per-app
  nanti — bukan sekarang.
- **Runaway loop tanpa host-call tak bisa di-preempt.** → Watchdog-thread
  `terminate()` (hard kill, `m3_FreeRuntime`); thread+runtime per-app terisolasi
  → bongkar satu tak ganggu lain. Deadline host-call menangkap sisanya.
- **Sandbox escape via pointer guest.** → `boundsOk()` wajib di **setiap** akses
  host ke memori guest (iovec/string/struct); wasm3 sudah bounds-check load/store
  guest. Review trampolin = checklist sandbox.
- **"wasm di dalam wasm" di simulator lambat/aneh.** → Diterima demi **satu
  jalur**; sim = test-ground device, bukan target performa. Menolak engine browser
  native (divergensi semantik) — keputusan terkunci.
- **WASI surface luas (banyak fd).** → Implementasi **minimal**: fd 0/1/2 saja,
  `fd_seek/fd_close` stub; tak ada filesystem WASI di v1 (storage lewat System API
  `nema:storage/*`, Plan 48).

---

## Yang sengaja TIDAK dikerjakan (sekarang)

- **AOT/JIT** (wasm3 interpret saja) — per-arch, non-portabel, melawan satu-jalur.
- **WASI filesystem/socket penuh** — fd 0/1/2 + exit; sisanya via System API (`nema:<domain>/<name>`, Plan 48).
- **Engine browser native untuk sim** — dua-jalur/divergensi, ditolak.
- **Multi-thread di dalam guest** (`wasi-threads`) — satu thread/app (Plan 22/55).
- **Sandbox memori bersama antar-app** — tiap app linear-memory terisolasi.
