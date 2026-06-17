# 48 — Nema System API: Core Interface Listing & IDL (Non-UI)

> Single source of truth untuk permukaan API **non-UI** yang di-expose
> kernel/services ke app — **bersama untuk semua display server**. Satu IDL →
> generate host + SDK + docs. "OpenAPI-nya Palanu." (UI = per-server, lihat Plan 50.)

- Status: ✅ Implemented — IDL di `api/*.pidl`, AST `api/build/nema-api.json`, codegen jalan (verifikasi 2026-06-15)
- Depends on: Plan 42 (capability), inventaris API saat ini
- Blocks: 49 (generator), 50 (UI SDK di-generate dgn toolchain sama), 56/57/58

---

## Goals

- Mendefinisikan **Nema System API (non-UI)**: interface bersama yang di-expose —
  `log, device, storage, profile, http, input, fs, wifi, ble, events,
  tasks/timer, power, audio, camera` (+ `subghz` future).
- Memilih/merancang **format IDL** (sintaks WIT-style atau JSON/TOML sendiri)
  sebagai SSOT, target flat-import (wasm3).
- Memetakan **interface ↔ capability** (Plan 42); menetapkan mana "core" vs gated.
- Merekonsiliasi penamaan (`net.wifi` vs `nema.wifi`) + versioning (major.minor).

## Keputusan

- IDL = SSOT; host registration, SDK (C/WASM/JS), docs, dan matriks parity
  semuanya **di-generate** dari sini (parity by construction).
- **UI dikeluarkan dari System API** → UI per-server (Plan 50/52). System API =
  permukaan non-UI bersama saja.
- IDL ≠ cermin `rt.*`: hanya subset terkurasi & aman; internal sistem
  (ServiceContainer, driver mentah, RemoteService, AppHostManager) **tidak** di-expose.
- Capability = izin import interface; interface core (`log/device/events/tasks`)
  selalu tersedia, sisanya gated.
- Sub-GHz mengikuti pola BLE (HAL + capability + service) saat ditambah.

---

## Latar belakang

Permukaan System API **sudah ada secara de-facto** — tapi ditulis tangan, satu
runtime saja (JS), dan tanpa SSOT. Sumber kebenaran sekarang adalah satu file:

- **`firmware/core/src/js/js_api.cpp`** = "System API v0". `JsEngine::installApi()`
  (js_api.cpp:117) membangun objek global `nema` (js_api.cpp:164 →
  `JS_SetPropertyStr(ctx, g, "nema", api)`) dengan tangan, satu `setFn(...)` per
  method. Yang ter-expose hari ini: `log` (js_api.cpp:29), `device.{name,caps,
  has,available}` (js_api.cpp:42-51,127-136), `storage.{get,set,remove}`
  (js_api.cpp:54-70), `http.get` (js_api.cpp:96), `profile.{userName,deviceName,
  hasPassword,verifyPassword}` (js_api.cpp:74-93).
- **Gating ditulis tangan** dan tidak konsisten: `http` muncul kalau
  `has(caps::NetHttp) || has(caps::NetWifi)` (js_api.cpp:146), `profile` muncul
  kalau `container().resolve<ProfileService>()` non-null (js_api.cpp:155),
  `storage` selalu ada (js_api.cpp:139). Tak ada tabel deklaratif interface↔cap.
- **Host diakses lewat `Runtime`** yang diteruskan ke engine via
  `JsEngine::setHost(rt, appId)` (js_engine.h:64). Method memanggil `e->host()->…`:
  `log()`, `capabilities()`, `config()`, `container().resolve<T>()`,
  `info()` (runtime.h:50-90).

**Masalah yang plan ini selesaikan:**

1. **Satu runtime, bukan tiga.** Plan App Platform menargetkan C built-in, WASM
   (wasm3), dan JS (QuickJS) di atas **satu Host API**. `js_api.cpp` hanya
   melayani JS dan tidak bisa dipakai ulang oleh wasm3/C tanpa menulis ulang
   semua marshalling. Tanpa SSOT, tiga runtime = tiga permukaan yang akan
   menyimpang (parity drift).
2. **Tak ada kontrak yang bisa di-generate.** Tipe `.d.ts`, header C SDK, docs,
   dan tabel registrasi semuanya harus konsisten — mustahil dijaga manual.
3. **Penamaan terpecah.** Capability pakai taksonomi `net.wifi`/`net.http`/
   `bt.ble`/`storage`/`profile` (capabilities.h:43-50). Prototype JS pakai
   `nema.http`, `nema.storage`, `nema.profile` (flat, tanpa domain). Dua skema
   ini harus disatukan sebelum permukaan dibekukan.
4. **Tak ada versioning.** Tidak ada `api_version`; app `.kapp` hanya membawa
   `needs: ["http"]` (Plan 37 §4.1, kapp.json). App lama di firmware baru (atau
   sebaliknya) tak terdeteksi.

**Inventaris sumber interface non-UI** (kandidat permukaan, dengan signature nyata):

| Domain | Sumber (file:line) | Bentuk nyata |
|---|---|---|
| log | `log/logger.h:21-26` | `info(component, msg, fields[])`, level trace…fatal |
| device/info | `system/system_info.h:7-17`, `system/capability_registry.h:32-39` | `boardName`, `has()`, `available()`, `list()` |
| storage.kv | `config/config_store.h:25-34` | `getString/getInt/setString/setInt/remove(ns,key)` (ns≤15, key≤15) |
| storage.fs | `fs/vfs.h`, `hal/filesystem.h:28-32` | `list/read/write/mkdir/remove(path)`, whole-file v1 |
| net.http | `hal/http_client.h:7-22` | `get(url,insecure)→{status,body}`, **blocking → TaskRunner** |
| net.wifi | `hal/wifi.h:30-43` | `connect/disconnect/isConnected/ssid/scan/scanResults/ip` |
| bt.ble | `hal/bluetooth.h:36-81` | controller + GATT peripheral + pairing + bonding |
| profile | `services/profile_service.h:29-39` | read-only `userName/deviceName/hasPassword/verifyPassword` |
| events | `event/event_bus.h:16-18` | `subscribe(name|"*")/unsubscribe/publish` |
| tasks/timer | `task_runner.h:33-37` | `submit(job,done)` worker→UI; basis `setTimeout/interval` |
| input | `services/input_service.h:24-55` | Action funnel, `hintFor(Action)` |
| audio | `services/audio_service.h:11-21` | enumerate input/output devices |
| camera | `services/camera_service.h:10-14` | enumerate + capture |
| power | `runtime.h:75` (`dpm()`) | sleep/lock state machine |

> **Catatan threading (mengikat IDL):** `http.get`, `wifi.scan/connect`, dan
> `ble.enable` **MEMBLOKIR** dan wajib jalan di worker `TaskRunner`
> (http_client.h:13, wifi.h:26-29, bluetooth.h:38). IDL menandai fungsi ini
> `@blocking` → generator membungkusnya jadi async (Promise di JS, callback di
> WASM) lewat `rt.tasks().submit()` (task_runner.h:33), bukan memanggil di UI loop.

### Pelajaran dari referensi

| Aspek | Flipper Momentum | AkiraOS (Zephyr) | Keputusan Palanu |
|---|---|---|---|
| SSOT permukaan API | `api_symbols.csv` (4670 baris) — daftar simbol C diekspor ke app ELF | Tidak ada IDL; host-call by index manual | **IDL `.pidl` (WIT-subset)** → satu generator |
| Bentuk SSOT | CSV flat simbol C (alamat fungsi) | — | Teks WIT-style → JSON AST kanonik |
| Versioning | `api_version` u16.u16 (major.minor) di header CSV (`Version,+,87.1`) + manifest (`application_manifest.h:27-35`) | `version` semver string app saja | **`api_version` major.minor** dicek saat load (ala Flipper) |
| Cek saat load | major harus sama, app.minor ≤ fw.minor → tolak | hanya nama+memory_quota | **Sama**: major exact, minor ≤ host |
| Deklarasi izin | hardware_target_id + flags | manifest JSON `capabilities:[...]` array | `.kapp` `needs:[caps]` (string cap, Plan 42) |
| Granularitas izin | per-app (semua API atau tidak) | per-capability string (`storage.write`) | **per-interface = 1 capability** (Plan 42) |
| Penyematan manifest | struct biner di ELF | WASM custom section `.akira.manifest` (JSON) | `.kapp` header line JSON (Plan 37 §2.1) |

**Kesimpulan:** Flipper benar soal **versioning u16.u16 + cek-saat-load**, tapi
SSOT-nya cuma daftar simbol (tak ada tipe/marshalling). AkiraOS benar soal
**capability per-string di manifest**, tapi tanpa IDL. Palanu menggabungkan:
**IDL bertipe** (records/enums/results, bisa generate marshalling) + **versioning
Flipper** + **capability per-interface AkiraOS**.

---

## Desain teknis

### 1. Format IDL — keputusan: **WIT-subset teks (`.pidl`) → JSON AST**

Tiga kandidat dipertimbangkan:

| Format | Pro | Kontra | Verdict |
|---|---|---|---|
| **WIT-subset teks** (`.pidl`) | `record`/`enum`/`result`/`option`/`list`, fungsi bertipe; ringkas & diff-friendly; ekosistem WASM kenal | butuh parser kecil (≈300 LOC bun) | ✅ **dipilih** (sumber) |
| JSON tulis-tangan | tak perlu parser | verbose, tak ada tipe ergonomis, rawan typo, jelek di-diff | ❌ tapi dipakai sebagai **AST hasil-parse** |
| TOML sendiri | enak untuk kunci datar | nested signature (param list, result) jadi array-of-table canggung | ❌ |

**Keputusan:** *author* di **WIT-subset** (`api/*.pidl`), parser bun mengubahnya
jadi **JSON AST** (`api/build/nema-api.json`) yang menjadi SSOT mesin yang
dikonsumsi generator (Plan 49). Best-of-both: sumber enak dibaca + AST mudah
di-emit. **Flat-import wasm3** dijaga dengan **membatasi** WIT: TIDAK ada
`world`/`instance`/instansiasi Component Model, TIDAK ada nested resource handle
sebagai tipe kaya — `resource` (mis. file handle) diekspor sebagai **`handle`
opaque (i32)** supaya cocok dengan tabel import datar wasm3.

Tipe primitif IDL → ABI per runtime:

| IDL | wasm3 ABI (flat) | QuickJS | C built-in |
|---|---|---|---|
| `u8/u16/u32/s32/u64/s64` | i32/i64 di stack | Number/BigInt | int |
| `f32/f64` | f32/f64 | Number | double |
| `bool` | i32 (0/1) | Boolean | bool |
| `string` | `(ptr,len)` ke linear mem | JS string | `const char*`/`std::string` |
| `list<T>` | `(ptr,len)` | Array | `span<T>` |
| `record` | pointer ke struct flat / multi-return | Object | struct |
| `result<T,E>` | `(ok_flag, val…)` atau error code | throw / `{ok,err}` | `expected`-like |
| `option<T>` | `(present, val)` | `T \| null` | `optional<T>` |
| `handle` | i32 (index ke tabel host) | Number opaque | int |

### 2. Penamaan kanonik (rekonsiliasi `net.http` vs `nema.http`)

Satu skema, diturunkan dari **taksonomi capability Plan 42** (capabilities.h):

```
Interface ID  : nema:<domain>/<name>@<major>.<minor>
Capability    : <domain>.<name>   (persis konstanta nema::caps::*)
JS path       : nema.<domain>.<name>
WASM import   : ("nema:<domain>/<name>" "<func>")   ; modul = interface id
C SDK symbol  : nema_<domain>_<name>_<func>()
```

Contoh konkret penyatuan:

| Lama (v0 prototype) | Capability (Plan 42) | **Kanonik (plan ini)** |
|---|---|---|
| `nema.http.get` | `net.http` | id `nema:net/http`, JS `nema.net.http.get` |
| `nema.storage.get` | `storage` | id `nema:storage/kv`, JS `nema.storage.kv.get` |
| `nema.device.has` | (core) | id `nema:sys/device`, JS `nema.sys.device.has` |
| `nema.profile.userName` | `profile` | id `nema:profile`, JS `nema.profile.userName` |

> **Domain tanpa sub-nama** (mis. `profile`) → `nema:profile` (name = domain),
> JS `nema.profile.*`. **Alias kompat:** generator boleh memancarkan shim
> deprecated `nema.http` → `nema.net.http` untuk **satu** rilis major; dihapus di
> major berikut. `global` tetap `nema` (js_api.cpp:164) — tak berubah.

### 3. Daftar interface final + pemetaan capability

**Core (selalu ada — tak ber-capability gate):**

| Interface ID | JS path | Sumber |
|---|---|---|
| `nema:sys/log` | `nema.sys.log` | logger.h |
| `nema:sys/device` | `nema.sys.device` | capability_registry.h, system_info.h |
| `nema:sys/events` | `nema.sys.events` | event_bus.h |
| `nema:sys/tasks` | `nema.sys.tasks` | task_runner.h (timer) |
| `nema:storage/kv` | `nema.storage.kv` | config_store.h (selalu ada, js_api.cpp:139) |

**Gated (import butuh capability):**

| Interface ID | Capability (`nema::caps`) | Sumber | Catatan |
|---|---|---|---|
| `nema:storage/fs` | `storage` (`Storage`) | vfs.h, filesystem.h | whole-file v1 |
| `nema:net/http` | `net.http` (`NetHttp`) | http_client.h | `@blocking` |
| `nema:net/wifi` | `net.wifi` (`NetWifi`) | wifi.h | `@blocking` |
| `nema:bt/ble` | `bt.ble` (`BtBle`) | bluetooth.h | `@blocking` enable |
| `nema:profile` | `profile` (`Profile`) | profile_service.h | read-only |
| `nema:input` | `input` (`Input`) | input_service.h | Action layer |
| `nema:media/audio` | `audio.output`/`audio.input` | audio_service.h | enumerate/play |
| `nema:media/camera` | `camera` (`Camera`) | camera_service.h | capture |
| `nema:sys/power` | (core, read) / gated set | runtime.h dpm() | sleep/lock |
| `nema:radio/subghz` | `radio.subghz` (future) | — | pola BLE |

> **Dikeluarkan dari System API** (bukan non-UI atau internal): `ViewDispatcher`,
> `Canvas`, `GuiService`, `AppHostManager`, `RemoteService`, `ServiceContainer`,
> driver mentah. UI = per-display-server (Plan 50/52).

### 4. Contoh IDL konkret — `nema:storage/kv` & `nema:net/http`

```wit
// api/storage.pidl
package nema:storage@1.0

/// Per-app persistent key-value store. Namespace = appId (host-injected).
/// Backed by NVS on device, in-memory map on simulator. ns/key ≤ 15 chars.
@core            // always available, no capability gate
interface kv {
    /// Read a string value. Absent key → none.
    get: func(key: string) -> option<string>
    /// Write a string value (commits immediately).
    set: func(key: string, value: string)
    /// Read a 64-bit int. Absent key → none.
    get-int: func(key: string) -> option<s64>
    set-int: func(key: string, value: s64)
    /// Delete a key. Returns true if it existed.
    remove: func(key: string) -> bool
}
```

```wit
// api/net.pidl
package nema:net@1.0

record http-response {
    status: u16,          // HTTP code, 0 = transport error
    body: string,         // truncated to host cap
}

/// Networked HTTP client. Maps to IHttpClient (hal/http_client.h).
@capability("net.http")           // import gated by Plan 42 capability
interface http {
    /// HTTPS GET. BLOCKS → host runs on TaskRunner worker, app sees a future.
    @blocking
    get: func(url: string) -> result<http-response, string>
    @blocking
    post: func(url: string, body: string, content-type: string)
        -> result<http-response, string>
}
```

Catatan pemetaan: `option<string>` ↔ `IConfigStore::getString` yang me-return
`bool` + out-param (config_store.h:26); `@core` ↔ `kv` selalu dipasang
(js_api.cpp:139); `@capability("net.http")` ↔ gate `has(NetHttp)` (js_api.cpp:146);
`@blocking` ↔ `client->get()` di worker (http_client.h:13, task_runner.h:33).

### 5. Contoh hasil generate (rincian penuh = Plan 49)

**(a) Host C++ — signature yang harus diimplementasi (parity-enforced):**
```cpp
// generated/host/nema_api.gen.h  — runtime-agnostic, satu kali tulis bodinya
struct HostApi {
    // nema:storage/kv
    virtual bool kv_get(string_view key, std::string& out) = 0;     // option → bool+out
    virtual void kv_set(string_view key, string_view value)  = 0;
    // nema:net/http  (@blocking → dijalankan generator di TaskRunner)
    virtual HttpResult http_get(string_view url) = 0;
};
```

**(b) WASM SDK — header C (flat import):**
```c
// generated/sdk/nema.h
__attribute__((import_module("nema:net/http"), import_name("get")))
extern nema_result_t nema_net_http_get(const char* url, size_t url_len,
                                       nema_http_response_t* out);
```

**(c) JS binding + .d.ts:**
```ts
// generated/sdk/nema.d.ts
declare namespace nema.net.http {
  interface HttpResponse { status: number; body: string }
  /** HTTPS GET (async — runs off the UI thread). */
  function get(url: string): Promise<HttpResponse>
}
declare namespace nema.storage.kv {
  function get(key: string): string | null
  function set(key: string, value: string): void
}
```

### 6. Skema versioning

```c
// generated/nema_api_version.h
#define NEMA_API_VERSION_MAJOR 1
#define NEMA_API_VERSION_MINOR 3     // bump tiap interface/func ditambah
#define NEMA_API_VERSION ((1u<<16)|3u)   // packed u16.u16 (pola Flipper)
```

- **Bump aturan (SemVer permukaan):** tambah interface/func/field **opsional** →
  `minor++`. Hapus/ubah-signature/rename → `major++`. (Sama dengan Flipper
  `api_version` u16.u16, application_manifest.h:27-35.)
- **Manifest `.kapp`** (perluasan Plan 37 kapp.json) menambah blok `api`:
  ```jsonc
  { "id": "com.me.weather", "name": "Weather", "version": "1.2.0",
    "api": { "major": 1, "minor": 2 },        // permukaan yang di-build
    "needs": ["net.http", "storage"] }         // capability (Plan 42)
  ```
  > Bentuk final: container `.kapp`→`.papp` dan blok `api` diformalkan jadi field
  > string `"api_version": "1.0"` (major.minor) di **Plan 59** (skema manifest +
  > packaging PAPP1). Semantik versioning di sini (major exact, minor ≤ host) tak
  > berubah — hanya representasi manifest yang dipindahkan ke Plan 59.
- **Cek saat load** (di `JsAppStore::installKapp` / loader, ala Flipper
  `flipper_application_validate_manifest`):
  ```
  if (app.api.major != HOST.major)          → REFUSE (incompatible ABI)
  else if (app.api.minor > HOST.minor)      → REFUSE (app needs newer fw)
  else                                       → OK (fw ≥ app, backward-compatible)
  ```
  Plus tiap cap di `needs` dicek `capabilities().has(cap)` (Plan 42); cap tak ada
  → app tetap load tapi interface itu absen (degradasi anggun, perilaku v0).

---

## Fase

- [ ] **Fase 1 — Tulis IDL + bekukan permukaan.** Authoring `.pidl` untuk **semua**
      interface yang sudah ada di `js_api.cpp` (log, device, storage/kv, net/http,
      profile) — paritas 1:1 dengan v0. Output: `api/*.pidl` + dokumen daftar
      interface ini. Belum ada generator. Uji: review manual signature ↔ js_api.cpp.
- [ ] **Fase 2 — Parser `.pidl` → JSON AST.** Parser bun kecil (subset WIT) →
      `api/build/nema-api.json`. Validasi: tipe dikenal, cap merujuk `nema::caps`
      yang ada, tak ada nama duplikat. Uji: snapshot AST + unit test parser
      (good/malformed). (Generator emit = Plan 49.)
- [ ] **Fase 3 — Versioning + manifest.** Tetapkan `api_version` awal `1.0`,
      tambah blok `api` ke schema `.kapp` (Plan 37), implementasi cek-saat-load di
      loader. Uji: app dgn major beda → ditolak; minor lebih tinggi → ditolak;
      minor lebih rendah → OK (host + WASM).
- [ ] **Fase 4 — Rekonsiliasi penamaan + alias.** Pindahkan `js_api.cpp` ke path
      kanonik (`nema.net.http`, `nema.storage.kv`, …) dengan shim alias deprecated
      utk satu rilis. Uji: app contoh lama (`nema.http`) tetap jalan via alias;
      app baru pakai path kanonik. (Penggantian penuh js_api.cpp by-generated =
      Plan 49 Fase akhir.)

---

## File yang disentuh

**Baru:**
- `api/sys.pidl`, `api/storage.pidl`, `api/net.pidl`, `api/profile.pidl`,
  `api/bt.pidl`, `api/media.pidl`, `api/input.pidl` — sumber IDL (SSOT).
- `api/build/nema-api.json` — AST hasil-parse (generated, di-commit/cache).
- `packages/idl/src/parser.ts` — parser `.pidl`→AST (bun). *(Generator emitter = Plan 49.)*
- `firmware/core/include/nema/app/api_version.h` *(atau generated)* — `NEMA_API_VERSION*`.

**Disentuh:**
- `firmware/core/src/js/js_api.cpp` — rename path kanonik + alias (Fase 4);
  akhirnya digantikan output generator (Plan 49).
- `packages/nema-app-sdk/src/manifest.ts` — tambah field `api:{major,minor}` ke
  `KappManifest` (Plan 37 §2.1).
- Loader `.kapp` (Plan 37 Fase 6 `JsAppStore::installKapp`) — cek `api_version` +
  `needs` saat load.
- `docs/plans/49-*` — mengonsumsi AST.

---

## Test

- **Parser:** unit test bun — `.pidl` valid → AST benar; malformed (tipe tak
  dikenal, cap asing, duplikat) → error jelas. Snapshot `nema-api.json`.
- **Parity v0:** assert setiap method `js_api.cpp` muncul di IDL dan sebaliknya
  (tak ada drift saat freeze Fase 1).
- **Versioning (host + WASM):** matriks load — (major beda → tolak), (minor app >
  host → tolak), (minor app ≤ host → OK), (cap di `needs` absen → interface absen,
  app tetap load).
- **Alias:** app contoh pakai `nema.http` lama → tetap jalan via shim; log
  deprecation muncul sekali.
- Build hijau host + WASM tiap fase; ESP32 build-only setelah Fase 3.

---

## Risiko & mitigasi

| Risiko | Dampak | Mitigasi |
|---|---|---|
| Parser WIT-subset jadi proyek sendiri (scope creep) | Toolchain telat | Subset KETAT: hanya `interface/func/record/enum/result/option/list/handle`. Tanpa `world`/generics/resource kaya. ≈300 LOC. |
| IDL menyimpang dari `js_api.cpp` sebelum generator ada | Parity palsu | Fase 1 freeze + test parity v0 (Test §2); generator (Plan 49) mengunci permanen. |
| Penamaan kanonik memecah app v0 yang sudah ada | App lama rusak | Alias deprecated 1 rilis major (Desain §2); contoh app di-port. |
| `@blocking` salah-tandai → UI freeze | Device hang | Tandai dari kontrak HAL nyata (http_client.h:13, wifi.h:26); generator WAJIB bungkus `@blocking` lewat `rt.tasks()` (task_runner.h). |
| `handle` opaque bocor lintas app | Keamanan/korupsi | Handle = index ke tabel per-engine (pola handler table js_engine.h:103); divalidasi host-side, bukan pointer mentah. |
| Versioning major bump tiap rilis (churn) | App sering ditolak | Aturan ketat: tambah opsional = minor; major hanya untuk break. Generator gagal-build kalau signature berubah tanpa bump (cek di Plan 49). |
