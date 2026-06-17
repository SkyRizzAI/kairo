# 49 — SDK Binding Generator & API Docs/Parity Toolchain

> Toolchain yang membaca IDL (Plan 48) dan men-generate binding host C++, SDK
> WASM (C/Rust/AS), SDK JS, docs human-readable, dan matriks parity.

- Status: ✅ Implemented — generator `packages/idl/`, output `generated/` (host quickjs `nema_api*.gen.*` + SDK `nema.d.ts`/`nema.h`) (verifikasi 2026-06-15)
- Depends on: 48
- Blocks: 56/57/58 (runtimes mengonsumsi SDK), docs

---

## Goals

- Generator IDL → (a) tabel import/registration C++ host, (b) header/SDK WASM,
  (c) binding QuickJS + `.d.ts`, (d) situs docs, (e) matriks coverage/parity.
- Menjamin **parity by construction** antar SDK WASM & JS.
- Pipeline build terintegrasi (bun/CMake), artifact ter-versi.

## Keputusan

- Satu generator, banyak target; **tidak ada binding ditulis tangan**.
- Target WASM = flat imports (wasm3), bukan Component Model penuh.
- Docs = artefak generate (seperti Swagger dari OpenAPI), bukan manual.

---

## Latar belakang

Hari ini binding ditulis tangan, satu runtime saja, di **`firmware/core/src/js/
js_api.cpp`**. Setiap method = blok C boilerplate:

- **Marshalling manual berulang.** `argStr(ctx, argv[0])` (js_api.cpp:22),
  `JS_NewString`, `JS_NewInt32`, `JS_NewBool`, `JS_NewObject` +
  `JS_SetPropertyStr` dipanggil tangan tiap fungsi. `api_http_get` (js_api.cpp:96-105)
  saja = 10 baris marshalling untuk membungkus satu `HttpResponse` (http_client.h:7).
- **Registrasi manual.** `setFn(ctx, obj, name, fn, argc)` (js_api.cpp:107) dipanggil
  satu per method di `installApi()` (js_api.cpp:117-166), termasuk membangun objek
  domain (`device`, `storage`, `http`, `profile`) dengan tangan.
- **Gating manual & tersebar.** `if (has(NetHttp) || has(NetWifi))` (js_api.cpp:146),
  `if (resolve<ProfileService>())` (js_api.cpp:155) — logika izin dijahit ke tiap
  blok, bukan diturunkan dari tabel.
- **Tipe SDK terpisah, drift bebas.** `packages/nema-app-sdk/src/system.d.ts`
  (Plan 37 §2.1) mendeklarasikan `nema.*` untuk TypeScript **dengan tangan** —
  tak ada yang menjamin ia cocok dengan `js_api.cpp`. Tambah satu method = sunting
  ≥2 file tanpa pengecekan silang.

**Untuk tiga runtime ini tak terkelola.** App Platform menargetkan **C built-in,
WASM (wasm3), JS (QuickJS)** di atas satu Host API (Plan 48). Menulis tangan
marshalling untuk wasm3 (stack i32/i64 + linear memory) **dan** QuickJS (JSValue)
**dan** C = 3× boilerplate yang dijamin menyimpang. Yang dibutuhkan: **satu
generator** yang membaca AST IDL (Plan 48, `api/build/nema-api.json`) dan
memancarkan semua sisi sekaligus → *parity by construction*.

Yang **sudah ada untuk di-reuse:**
- AST IDL + parser (Plan 48 Fase 2) — input generator.
- `HandlerRef`/handler table (js_engine.h:73,103) — pola index→callback yang dipakai
  ulang untuk `handle` opaque & async completion.
- `TaskRunner::submit(job,done)` (task_runner.h:33) — target wrap `@blocking`.
- `JsEngine::setHost(rt, appId)` (js_engine.h:64) — titik injeksi global `nema`.
- Build: `packages/nema-app-sdk` sudah pakai **Bun.build** (Plan 37 Fase 0) →
  generator ikut runtime bun yang sama. CMake sudah jalankan langkah codegen
  (Plan 37 Fase 5 `gen-embedded-apps` → C header) — pola custom-command-pra-compile
  sudah terbukti di repo.

### Pelajaran dari referensi

| Aspek | Flipper Momentum | AkiraOS (Zephyr) | Keputusan Palanu |
|---|---|---|---|
| Sumber binding | `api_symbols.csv` → `arm-none-eabi` symbol table di-generate ke `.api` | host-call ditulis tangan per fungsi | **IDL AST → generator multi-target** |
| Marshalling | Linker resolve simbol; tipe = C ABI mentah (tak ada cek) | manual cast dari wasm stack | **Generated** per runtime (wasm3/QuickJS/C) |
| SDK app | header C OFW di-checkout, app link ke `.api` | header C tulis-tangan | header C + crate Rust + `.d.ts` **semua generated** |
| Docs | manual (wiki) | manual (md) | **Generated** dari doc-comment IDL |
| Parity check | `Version` bump + CI diff `api_symbols.csv` | tak ada | **Parity matrix generated** + build gagal jika impl host kurang |
| Toolchain | Python (`fbt`) | CMake/Kconfig | **bun (TS)** + CMake custom command |

**Kesimpulan:** Flipper sudah codegen dari SSOT (CSV→`.api`) dan punya CI-diff
versioning — tapi tabelnya tak bertipe, jadi marshalling tetap tanggung jawab dev.
Palanu naik level: IDL **bertipe** → marshalling **ikut di-generate** untuk 3
runtime, dan parity dijadikan **error build**, bukan konvensi.

---

## Desain teknis

### 1. Pipeline

```
 api/*.pidl ──parser(Plan48)──► api/build/nema-api.json  (AST = SSOT mesin)
                                          │
                              tools/idl/gen.ts  (bun, satu generator)
        ┌──────────────┬──────────────┬───────────┴──────┬──────────────┬───────────┐
        ▼              ▼              ▼                  ▼              ▼           ▼
   (a) host C++   (b) WASM SDK   (c) JS binding     (d) docs       (e) parity   version
   register +     C hdr + Rust   QuickJS glue +     md/html        matrix       header
   marshalling    crate          .d.ts                              md
        │              │              │                  │              │           │
        └─ generated/host/  generated/sdk/   generated/sdk/   docs/api/  docs/api/  generated/
```

Semua output ke `generated/` (di-gitignore atau di-commit ber-cache); **tak ada
file di `generated/` yang boleh disunting tangan** (header berstempel `// @generated
from api/build/nema-api.json — DO NOT EDIT`).

### 2. Pembagian generated vs hand-written (kunci "no hand-written bindings")

Yang **di-generate** = semua *glue*: registrasi, marshalling arg↔ABI, gating,
pembungkus async `@blocking`, tipe. Yang **ditulis tangan SEKALI** = *implementasi
semantik* yang menyentuh `rt.*` nyata — tapi signature-nya **didikte IDL** dan
**diverifikasi** (parity by construction).

```
            IDL func: nema:net/http.get(url) -> result<http-response,string> @blocking
                                   │ generate
        ┌──────────────────────────┼─────────────────────────────────────────┐
        ▼                          ▼                                          ▼
  generated marshalling     generated registration          generated HostApi signature
  (JSValue↔C++, wasm        (setFn / wasm3 import bind,      virtual HttpResult
   stack↔C++), wrap          gating dari capability)          http_get(string_view url)=0;
   @blocking via TaskRunner                                          │ harus diimplementasi
                                                                     ▼
                                              hand-written: NemaHostImpl : HostApi
                                              http_get(){ return rt.container()
                                                .resolve<IHttpClient>()->get(...); }
```

- **C built-in runtime** memanggil `HostApi` **langsung** (tanpa marshalling) — jadi
  ia ikut paritas gratis.
- **Parity by construction:** generator memancarkan `HostApi` sebagai kelas
  abstrak *pure virtual* dengan SATU method per func IDL. `NemaHostImpl` yang tak
  mengimplementasi sebuah method → **gagal kompilasi** (kelas abstrak). Tambah func
  ke IDL tanpa impl → build merah. Itulah "parity by construction".

### 3. Target generate (rinci)

**(a) Host C++ — registrasi + marshalling.**
- `generated/host/nema_api.gen.h` — `struct HostApi { virtual … =0; }` (kontrak).
- `generated/host/nema_api_quickjs.gen.cpp` — untuk tiap interface, fungsi
  `JSValue` yang: unmarshal arg (`argStr` dll, pola js_api.cpp:22), panggil
  `host->FUNC(...)`, marshal balik; `installNemaApi(ctx, HostApi&, CapabilityRegistry&)`
  membangun global `nema` + **gating dari tabel** (`if cap → pasang interface`),
  menggantikan `installApi()` tulis-tangan (js_api.cpp:117).
- `generated/host/nema_api_wasm3.gen.cpp` — `m3_LinkRawFunction` per func; baca arg
  dari stack/linear-mem, panggil `host->FUNC`, tulis balik.
- `@blocking` → marshalling membungkus panggilan dalam `rt.tasks().submit(job, done)`
  (task_runner.h:33): JS dapat `Promise`, WASM dapat pola callback/poll.

**(b) WASM SDK.**
- `generated/sdk/nema.h` — C header: tiap func = `extern` ber-`import_module`/
  `import_name` = interface id + func (lihat Plan 48 §5b).
- `generated/sdk/nema_sys.rs` *(opsional)* — crate Rust `extern "C"` + wrapper aman.

**(c) JS SDK.**
- `generated/sdk/nema.d.ts` — namespace bertingkat `nema.<domain>.<name>`
  (Plan 48 §5c); `@blocking`→`Promise<T>`. Menggantikan `system.d.ts` tulis-tangan.

**(d) Docs.**
- `docs/api/<interface>.md` dari doc-comment `///` di `.pidl` — signature, tipe,
  capability, flag `@blocking`, `@core`, `since` (api_version saat ditambah).
- Indeks `docs/api/index.md` + badge versi.

**(e) Parity matrix.**
- `docs/api/parity.md` — tabel `func × {host C, wasm SDK, JS SDK, docs, impl}` dgn
  ✅/—. Karena semua satu-generator, kolom binding selalu ✅; kolom **impl** dicek
  silang dgn simbol di `NemaHostImpl` (link/AST scan) → ✅ hanya jika benar ada.

### 4. Contoh output marshalling (QuickJS) — `nema:net/http.get`

```cpp
// generated/host/nema_api_quickjs.gen.cpp   // @generated — DO NOT EDIT
static JSValue nema_net_http_get(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* e = engineOf(ctx);
    std::string url = jsToString(ctx, argv[0]);             // gen marshalling
    return e->blockingAsync(ctx, [host=e->hostApi(), url] { // @blocking → TaskRunner
        return host->http_get(url);                          // hand-written impl
    }, [](JSContext* c, const HttpResult& r) {               // gen result<>→JS
        if (!r.ok) return JS_Throw(c, jsStr(c, r.err));
        JSValue o = JS_NewObject(c);
        JS_SetPropertyStr(c, o, "status", JS_NewInt32(c, r.value.status));
        JS_SetPropertyStr(c, o, "body",   jsStr(c, r.value.body));
        return o;
    });
}
// registration (gated):
if (caps.has(nema::caps::NetHttp))
    installInterface(ctx, root, "net", "http", { {"get", nema_net_http_get, 1}, … });
```

Bandingkan: blok ini hari ini ditulis tangan di js_api.cpp:96-105,146-150 untuk JS
saja; generator memancarkannya untuk JS **dan** wasm3 dari satu deklarasi IDL.

### 5. Integrasi build (bun + CMake)

```cmake
# CMakeLists (core) — codegen sebelum compile (pola gen-embedded-apps, Plan 37 F5)
add_custom_command(
  OUTPUT  ${GEN}/host/nema_api_quickjs.gen.cpp ${GEN}/sdk/nema.d.ts ...
  COMMAND bun run ${CMAKE_SOURCE_DIR}/tools/idl/gen.ts
          --ast ${CMAKE_SOURCE_DIR}/api/build/nema-api.json --out ${GEN}
  DEPENDS ${CMAKE_SOURCE_DIR}/api/build/nema-api.json ${IDL_GEN_SOURCES})
add_custom_target(nema_api_gen DEPENDS ${GEN}/host/nema_api_quickjs.gen.cpp)
```
- `bun run gen` juga dipanggil di pipeline `packages/nema-app-sdk` agar `.d.ts`
  ikut ter-publish ke SDK npm. Satu generator, dipanggil dari dua build (firmware
  CMake + SDK bun) tapi **output deterministik & ber-stempel**.
- CI: `bun run gen --check` (regenerate ke tmp, `diff` dgn commit) → gagal kalau
  generated stale (memaksa `api_version` bump + commit ulang, pola Flipper CI).

---

## Fase

- [ ] **Fase 1 — Generator skeleton + (d) docs + (e) parity.** Baca AST (Plan 48
      Fase 2), emit `docs/api/*.md` + `parity.md`. Target paling tak-berisiko, nilai
      cepat. Uji: docs match daftar interface Plan 48.
- [ ] **Fase 2 — (a) Host: `HostApi` + QuickJS glue, ganti `js_api.cpp`.** Emit
      `nema_api.gen.h` + `nema_api_quickjs.gen.cpp` + `NemaHostImpl` (hand-written,
      thin di atas `rt.*`). Hapus `installApi()` tulis-tangan; `setHost` panggil
      `installNemaApi`. **Paritas v0 harus identik.** Uji: app JS contoh (counter,
      sysinfo, http) jalan tanpa perubahan (host + WASM).
- [ ] **Fase 3 — (c) JS `.d.ts` + (b) WASM C header.** Ganti `system.d.ts`
      tulis-tangan dgn generated; emit `nema.h`. Uji: SDK TS compile dgn tipe
      generated; app WASM stub link ke import flat (wasm3 resolve).
- [ ] **Fase 4 — Async `@blocking` + parity CI.** `blockingAsync` (Promise via
      TaskRunner) untuk http/wifi/ble; `bun run gen --check` di CI; build merah jika
      `HostApi` kurang impl atau generated stale. Uji: http async tak freeze UI
      (sim); hapus 1 impl → build gagal (bukti parity-by-construction).
- [ ] **Fase 5 — Rust crate + wasm3 binding penuh.** `nema_sys.rs` + emit
      `nema_api_wasm3.gen.cpp` lengkap (saat runtime WASM Plan 57 mendarat). ESP32
      build-only.

---

## File yang disentuh

**Baru:**
- `packages/idl/src/gen.ts` — generator multi-target (bun). *(parser = Plan 48.)*
- `packages/idl/src/emit/{host_cpp,quickjs,wasm3,dts,rust,docs,parity}.ts` — emitter per target.
- `generated/host/nema_api.gen.h`, `nema_api_quickjs.gen.cpp`, `nema_api_wasm3.gen.cpp`
  — output host (gitignore atau commit-cache).
- `generated/sdk/nema.h`, `nema.d.ts`, `nema_sys.rs` — output SDK.
- `firmware/core/src/js/nema_host_impl.cpp` — `NemaHostImpl : HostApi`,
  satu-satunya kode tangan yang menyentuh `rt.*`.
- `docs/api/*.md` + `docs/api/parity.md` — docs/parity generated.

**Disentuh:**
- `firmware/core/src/js/js_api.cpp` — **dihapus/diciutkan**; `installApi()` digantikan
  `installNemaApi()` generated. `JsEngine::setHost` (js_engine.h:64) memanggilnya.
- `firmware/core/include/nema/js/js_engine.h` — `installApi()` → `installNemaApi`;
  tambah `hostApi()`/`blockingAsync()` helper.
- `packages/nema-app-sdk/src/system.d.ts` — diganti generated `nema.d.ts`.
- `CMakeLists.txt` (core) + `packages/nema-app-sdk` build — custom command codegen.
- CI workflow — langkah `bun run gen --check`.

---

## Test

- **Generator unit (bun):** AST contoh kecil → output deterministik per emitter
  (snapshot). Idempoten (jalankan 2× = identik).
- **Parity v0 (host + WASM):** counter/sysinfo/http (Plan 37) jalan **tanpa
  perubahan** setelah `js_api.cpp` diganti generated. Output visual & API identik.
- **Parity-by-construction:** hapus satu method dari `NemaHostImpl` → **build
  gagal** (kelas abstrak). Tambah func IDL tanpa impl → build gagal. (Test eksplisit.)
- **`--check` CI:** sunting `.pidl`, lupa regenerate → `gen --check` merah.
- **Async:** app JS `await nema.net.http.get(...)` → UI tetap tick (FPS tak nol)
  selama request (sim).
- **`.d.ts`:** TS app SDK compile dgn tipe generated; salah-pakai (mis. `get()`
  tanpa await pada `@blocking`) → tipe error.
- Build hijau host + WASM tiap fase; ESP32 build-only Fase 2 & 5.

---

## Risiko & mitigasi

| Risiko | Dampak | Mitigasi |
|---|---|---|
| Generator jadi monolit susah-rawat | Toolchain rapuh | Emitter terpisah per target (`emit/*.ts`); AST stabil = kontrak; tiap emitter < ~200 LOC. |
| Output generated di-edit tangan lalu hilang saat regen | Bug senyap | Stempel `// @generated — DO NOT EDIT`; `gen --check` di CI; impl tangan HANYA di `nema_host_impl.cpp`. |
| Marshalling generated ≠ perilaku js_api.cpp v0 | App lama rusak | Fase 2 target paritas byte-identik; test app contoh Plan 37 sebagai gerbang. |
| ABI wasm3 (stack/linear-mem) salah → app WASM crash | Runtime gagal | Emitter wasm3 ditunda ke Fase 5 (saat runtime WASM Plan 57 ada); host/JS dulu. |
| `@blocking`→Promise marshalling bocor/lifetime | UAF / freeze | `blockingAsync` reuse pola `TaskRunner::submit(job,done)` (task_runner.h:33) + handler-table lifetime (js_engine.h:103); shared-state by value. |
| Codegen memperlambat build inkremental | DX buruk | Custom command `DEPENDS` AST saja → regen hanya saat IDL berubah; output deterministik = cache-friendly. |
| Dua pemanggil generator (CMake + bun SDK) divergen | Drift output | Satu `gen.ts`, satu AST input, output deterministik ber-stempel; CI `--check` jadi wasit. |
