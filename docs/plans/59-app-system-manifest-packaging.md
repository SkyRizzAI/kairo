# 59 — App System: Manifest, Packaging (.papp) & Launcher

> Membungkus ketiga runtime jadi produk: manifest, format single-file & bundle
> `.papp` (ala macOS .app), install/registry/launcher, persistence.

- Status: 🟧 Detail draft (belum diimplementasi)
- Depends on: 55, 56, 57, 58; melanjutkan Plan 37 (custom apps), 38 (appstore)

---

## Goals

- Skema **manifest**: id, name, version, entry, **runtime** (c/wasm/js),
  **display_server** (`headless` | `aether`), **mode** (cli/ui/hybrid),
  `needs[]`=capabilities, icon, kategori + versioning API.
- Dua amplop: **single-file script** (Unix-style) & **bundle `.papp`** (folder
  authoring + assets/icon, di-pack ke PAPP1/TOC — bukan zip, lihat §3) — satu objek
  app runtime di belakang.
- Install/registry/launcher (lanjut Plan 37/38), persistence (LittleFS), icon di
  launcher, capability declaration & gating saat launch.

## Keputusan

- `.kapp`/KAPP1 → `.papp`/PAPP1; bundle = authoring form; transfer/disk form =
  **TOC-concatenated (PAPP1), BUKAN zip** (diputuskan di §3) untuk streaming via KLP.
- Single-file vs bundle = packaging berbeda, **runtime sama**.
- **UI app deklarasi `display_server` target** (sekarang `aether`); `headless` jalan
  di board mana pun. Launch gating = capability + ketersediaan server target (Plan 51).

---

## Latar belakang

Prototype Plan 37 sudah punya separuh dari sistem ini, tapi sengaja minimalis:

- **Container `.kapp`/KAPP1** = file teks `KAPP1\n<manifest-json-line>\n<js-bundle>`
  (lihat `bin/nema-build.ts:29`). Parsing di device (`js_app_store.cpp:40`
  `installKapp`) = split dua newline + ekstrak field via `jsonField()` (cari
  `"key":"value"` literal — bukan parser JSON beneran). Hanya menampung **satu blob
  JS**; tak ada tempat untuk icon, assets, atau multi-file.
- **Manifest** (`kapp.json`) cuma `id, name, version, entry, needs[]`. Tak ada
  `runtime` (di-asumsikan JS), tak ada `display_server`/`mode` (di-asumsikan
  UI-Aether), tak ada `icon`/`category`/`api_version`.
- **Registry** (`app_manifest.h`/`app_registry.h`) menyimpan `id, name, version,
  kind (BuiltIn|Custom), type (App|Service)` — cukup untuk launcher daftar-nama,
  belum cukup untuk icon, runtime tier, atau gating display-server.
- **Transfer** = `ExtOp::AppInstall = 0x03` di channel PLP `EXT` (Plan 35):
  host kirim **byte `.kapp` mentah** → `controlFn_` platform → `installKapp`.
  **Volatile** (RAM, hilang saat reboot). Persistensi LittleFS = Plan 38
  (`/flash/apps/*.kapp`), belum ada.

Plan 59 menaikkan ini ke produk: satu **skema manifest** yang menampung 3 runtime
(C/WASM/JS) + 2 display target (headless/aether), satu **container `.papp`/PAPP1**
yang bisa membawa code + manifest + icon + assets, dan **launcher** yang
menggambar icon serta menggating launch berdasar capability (Plan 42) +
ketersediaan display-server (Plan 51). Runtime di belakang tetap satu objek app
(Plan 56) — packaging berubah, eksekusi tidak.

### Pelajaran dari referensi

Tiga sumber, satu pertanyaan: *bagaimana app mendeklarasikan diri & dibungkus.*

| Aspek | Flipper `.fam` / FAP | AkiraOS (WASM) | **Keputusan Palanu** |
|---|---|---|---|
| Bentuk manifest | Python DSL `App(...)` di `application.fam`, di-compile fbt | JSON di **custom-section WASM** `.akira.manifest` atau `.json` eksternal | **JSON** (`manifest.json`) — JS-friendly, satu parser, manusiawi |
| ID app | `appid="snake_game"` | `name` (≤31 char, `[A-Za-z0-9_]`) | `id` reverse-DNS `com.palanu.clock` (sudah dipakai) + `name` display |
| Tipe runtime | implisit (native ARM thumb) | implisit (wasm32) | **`runtime: c\|wasm\|js`** eksplisit (kita punya 3 tier) |
| Entry | `entry_point="snake_game_app"` (symbol) | export `_start` (WASI) | `entry` = file sumber (authoring); artifact di-resolve builder per runtime |
| Kategori/UI | `apptype` (EXTERNAL/SERVICE/...), `fap_category` | — | **`type` (App\|Service)** + **`category`** + **`mode` (cli\|ui\|hybrid)** |
| Capability/izin | `requires=["gui"]`, `stack_size` | `capabilities:[]` + alias/wildcard (`storage.*`) + `memory_quota` | **`needs:[]`** = katalog Plan 42 (`net.wifi`,`storage`,…); quota dari runtime |
| Icon | `fap_icon="x_10px.png"` (di-compile ke C) | — | **`icon`** = path di bundle (1-bit pixelete, di-render Aether) |
| Assets | `fap_file_assets="files"` (folder → SD) | — | folder `assets/` di bundle `.papp`; di-resolve Aether asset-pack (Plan 53) |
| Versioning API | API-version harness fbt (ABI) | — | **`api_version`** = versi IDL System-API (Plan 48), dicek saat launch |
| Bungkus/transfer | FAP = ELF, di-load dynamic; assets via SD | `.wasm` (manifest embedded) | **`.papp`/PAPP1** = TOC-concatenated (lihat §Desain) |

Insight yang diambil:

1. **Manifest = JSON** (AkiraOS), bukan DSL bahasa (Flipper) — satu parser, bisa
   dibaca host & device, ramah JS. Tapi **bukan** embedded-in-binary (AkiraOS
   custom-section) karena runtime kita ada 3 (C tak punya "binary" yang ikut
   dikirim) → manifest = **entry pertama** di container, selalu bisa dibaca tanpa
   memuat code.
2. **Capability sebagai array string + katalog** (AkiraOS) = persis model Plan 42
   kita. Kita **tolak** alias/wildcard `*` AkiraOS (terlalu longgar; AkiraOS
   sendiri mencatat `"admin"` tak ditolak) — `needs[]` harus eksplisit per
   capability.
3. **Icon + assets sebagai file dalam bundle** (Flipper `fap_icon`/`fap_file_assets`)
   = yang prototype JS belum punya; bundle `.papp` memberi tempatnya.
4. **`runtime`/`mode`/`display_server` eksplisit** = yang tak satu pun referensi
   butuh (mereka single-runtime, single-display) tapi **wajib** untuk Palanu:
   tiga tier runtime + headless-vs-aether.

---

## Desain teknis

### 1. Skema manifest (`manifest.json`)

Satu dokumen JSON. Untuk built-in (C) ia hidup sebagai metadata di tabel embedded;
untuk custom (JS/WASM) ia adalah **entry pertama** di container `.papp`.

```jsonc
{
  "schema": "papp/1",            // versi format manifest (bukan versi app)
  "id": "com.palanu.clock",      // identitas unik, reverse-DNS (SSOT registry)
  "name": "Clock",               // nama tampil di launcher
  "version": "1.2.0",            // semver app
  "api_version": "1.0",          // versi System-API IDL (Plan 48) yg di-target

  "runtime": "js",               // c | wasm | js  (tier runtime, Plan 56/57/58)
  "entry": "App.tsx",            // file sumber (authoring); artifact di-resolve builder
  "mode": "ui",                  // cli | ui | hybrid
  "display_server": "aether",    // headless | aether
  "display_server_version": "1.0", // opsional: versi UI SDK aether:ui yg di-target (Plan 50/51)

  "needs": ["net.http", "storage"],   // capabilities (katalog Plan 42)
  "type": "app",                 // app (launchable) | service (daemon, Plan 19.6)
  "category": "Tools",           // grup di launcher (Flipper fap_category)

  "icon": "icon.pbm",            // path dlm bundle (1-bit pixelete); opsional
  "author": "Palanu",            // metadata, opsional
  "description": "On-device clock" // opsional
}
```

**Aturan field**

| Field | Wajib | Nilai | Catatan |
|---|---|---|---|
| `schema` | ya | `"papp/1"` | versi *container/manifest*. Mismatch major → tolak install. |
| `id` | ya | reverse-DNS | kunci registry; re-install id = replace (sesuai `installApp`). |
| `name` | ya | string | fallback ke `id` bila kosong (perilaku `installKapp` sekarang). |
| `version` | ya | semver | default `"1.0.0"` (perilaku sekarang). |
| `api_version` | ya | `major.minor` | dicek vs versi IDL host (lihat §Gating). default `"1.0"`. |
| `runtime` | ya | `c`\|`wasm`\|`js` | menentukan tier loader (Plan 56/57/58). |
| `entry` | (js/wasm) | path sumber | builder meng-compile → artifact (`app.js`/`app.wasm`). C: tak ada. |
| `mode` | ya | `cli`\|`ui`\|`hybrid` | `cli`=stdio only; `ui`=butuh surface; `hybrid`=keduanya. |
| `display_server` | ya | `headless`\|`aether` | `headless` jalan di mana pun; `aether` butuh server (Plan 51). |
| `display_server_version` | tidak | `major.minor` | versi UI SDK `aether:ui` (Plan 50/52) yg di-target. Major mismatch vs `uiSdk()->versionMajor` → UI ditolak saat negosiasi (Plan 51). Default = versi server. |
| `needs` | tidak | array capability | default `[]`. Eksplisit, **tanpa wildcard**. |
| `type` | tidak | `app`\|`service` | default `app`. `service`=hidden, ke ServiceManager. |
| `category` | tidak | string | default `"Apps"`. |
| `icon` | tidak | path bundle | 1-bit; default = icon generik per-runtime di launcher. |
| `author`,`description` | tidak | string | metadata; tak memengaruhi eksekusi. |

**Konsistensi `mode` × `display_server`** (divalidasi saat install):

| `mode` | `display_server` | Arti | Valid? |
|---|---|---|---|
| `cli` | `headless` | proses stdio murni (pipe-able) | ✅ |
| `ui` | `aether` | klien Aether (minta surface) | ✅ |
| `hybrid` | `aether` | jalan di shell **dan** bisa angkat UI | ✅ |
| `ui` | `headless` | UI tanpa server | ❌ tolak |
| `cli` | `aether` | (redundan) — turunkan ke `headless` | ⚠️ normalisasi |

> `runtime` ≠ `display_server`. `runtime` = bahasa eksekusi (C/WASM/JS). `display_server`
> = ke server UI mana app terikat. App JS bisa `headless` (CLI), app C bisa `aether`.

### 2. Dua amplop, satu runtime

Authoring punya dua bentuk; **keduanya menghasilkan satu `PappPackage`** di device
(satu `IApp` di belakang — sesuai keputusan "packaging beda, runtime sama").

**(a) Single-file script (Unix-style)** — untuk app cepat tanpa asset. Ini
generalisasi langsung dari KAPP1 sekarang:

```
PAPP1\n
<manifest-json-line>\n
<code-bundle (js minified | wasm base? → lihat catatan)>
```

- Cocok untuk `runtime: js` (code = teks). Tak ada icon/assets. `display_server`
  default `aether`, `mode` `ui` (kompat KAPP1). 1 entry implisit: `app.js`.
- Drop-in pengganti `.kapp`: `installPapp()` mendeteksi bentuk single-file
  (tepat 2 newline sebelum body) vs bundle (TOC) dari magic + struktur.

**(b) Bundle `.papp`** — folder yang di-pack ke satu file container. Authoring:

```
clock.papp/                 (folder authoring; di-pack jadi satu file .papp)
├── manifest.json
├── App.tsx                 (sumber; tak ikut ke device)
├── app.js                  (artifact build — yg dikirim)
├── icon.pbm                (1-bit, pixelete)
└── assets/
    ├── font.bin
    └── beep.raw
```

`papp-build <dir>` (ganti `nema-build.ts`): baca `manifest.json`, compile `entry`
→ artifact (`app.js` via Bun.build seperti sekarang, atau `app.wasm`), lalu
**pack** `manifest.json` + artifact + `icon` + `assets/**` jadi satu container
PAPP1 (lihat format §3). Single-file = mode `--script` (skip assets/icon).

### 3. Format transfer — **KEPUTUSAN: TOC-concatenated (PAPP1), BUKAN zip**

Container `.papp` = **header + table-of-contents + blob ter-konkat**, bukan zip:

```
offset  field
0       magic      "PAPP1\n"        (6 byte, ASCII — manusiawi, mudah sniff)
6       u16  entryCount             (LE)
8       TOC[entryCount], tiap entry:
          u8   nameLen
          u8[] name                 ("manifest.json" | "app.js" | "icon.pbm" | "assets/font.bin")
          u8   flags                (bit0 = COMPRESSED/RLE — reuse PLP RLE)
          u32  length               (LE, byte blob ter-dekompres = ukuran asli)
          u32  stored               (LE, byte di file; == length bila tak ter-kompres)
…       blob[0] blob[1] … blobN     (ter-konkat, urutan = TOC; tiap `stored` byte)
```

Invariant: **`manifest.json` WAJIB entry ke-0** → launcher/registry bisa baca
metadata + icon tanpa memuat code/assets.

**Kenapa TOC, bukan zip (alasan MCU):**

| Kriteria | zip (DEFLATE) | **TOC-concat (PAPP1)** |
|---|---|---|
| Library | butuh miniz/zlib (~20–40KB flash) + RAM window inflate (~32KB) | **nol** — parser = offset aritmatika (kode kita sendiri) |
| Streaming via PLP/BLE | **buruk**: central directory di **akhir** file → harus buffer seluruh file dulu / butuh seek | **baik**: header+TOC di **depan** → tulis tiap blob ke LittleFS saat byte tiba |
| Dekompresi | DEFLATE = CPU + heap window per file | opsional **RLE per-entry** (codec PLP yg sudah ada, byte-stream) |
| Reuse infra | tak ada | sama persis pola KAPP1 (split offset) + RLE Plan 35 |
| Random access on-device | perlu, tapi mahal | TOC = offset langsung ke blob (mmap/seek LittleFS) |
| Auditability | binary, butuh tool | magic ASCII + nama file plaintext di TOC |

> ZIP menang hanya kalau kita butuh kompres lintas-file & tooling desktop. Di MCU
> dengan transport byte-stream (PLP) + LittleFS, **central-directory-di-akhir**
> adalah killer: tak bisa install secara streaming (harus tahan seluruh `.papp` di
> RAM untuk cari TOC zip). TOC-di-depan membalik itu: parse header → untuk tiap
> entry, salin `stored` byte langsung ke `/flash/apps/<id>/<name>`. Kompresi yang
> kita perlukan (icon/teks JS berulang) ditangani **RLE per-entry** yang codec-nya
> **sudah ada & teruji** (Plan 35, `klp_codec`).
>
> Author boleh saja meng-zip folder untuk distribusi web (manusiawi), tapi **bentuk
> di-kirim-via-KLP & bentuk-di-disk adalah PAPP1/TOC** — `papp-build` mengeluarkan
> itu. Tak ada inflater di firmware.

**Wire (PLP)**: ganti semantik `ExtOp::AppInstall` (0x03) dari "raw `.kapp`" jadi
"raw `.papp` (PAPP1)". Frame > MTU sudah di-fragment oleh PLP (`FRAG_MORE`,
Plan 35) — device me-reassembly lalu `installPapp(bytes,len)`. Untuk paket besar
(assets), opsi streaming: opcode `AppInstallBegin/Chunk/End` (host kirim TOC dulu,
lalu tiap blob) sehingga device menulis ke LittleFS tanpa menahan seluruh paket di
RAM. v1 = reassembly-penuh (paket kecil); streaming = peningkatan additif.

### 4. Persistence (LittleFS) & registry

Saat install, **explode** container ke direktori per-app (bukan simpan blob utuh):

```
/flash/apps/<id>/
├── manifest.json
├── app.js | app.wasm        (artifact runtime)
├── icon.pbm
├── assets/…
└── data/                    (sandbox tulis app — palanu.storage, Plan 38)
```

- **Boot**: `loadEmbeddedJsApps` (built-in C/JS) **lalu** scan `/flash/apps/*/manifest.json`
  → `installPapp` dari disk (lanjutan `JsAppStore::loadPersisted`, Plan 38 Fase 2).
- **Install OTA**: `installPapp(bytes)` → explode ke `/flash/apps/<id>/` → daftar ke
  registry → muncul di launcher. Bila LittleFS absen (WASM/host tanpa MemFs) →
  volatile (perilaku Plan 37 Fase 6 tetap, app hilang saat reboot).
- **Explode** = launcher baca `manifest.json`+`icon` murah (tak load code); runtime
  mmap artifact; Aether resolve `assets/` via asset-pack (Plan 53).

**Registry** (`app_manifest.h`) diperluas dari 5 field jadi metadata penuh:

```cpp
struct AppManifest {
    const char* id;
    const char* name;
    const char* version;
    AppKind     kind;          // BuiltIn | Custom            (ada)
    AppType     type;          // App | Service              (ada)
    // — baru (Plan 59) —
    RuntimeTier  runtime;      // CBuiltin | Wasm | Js — reuse enum Plan 56 (BUKAN enum baru)
    DisplayTarget server;      // Headless | Aether
    AppMode      mode;         // Cli | Ui | Hybrid
    const char*  category;     // "Tools"
    const char*  iconPath;     // "/flash/apps/<id>/icon.pbm" | nullptr
    ApiVersion   apiVersion;   // {major, minor} — versi System-API IDL (Plan 48)
    ApiVersion   serverVersion;// display_server_version (aether:ui), opsional (Plan 51)
    CapList      needs;        // span<const char*> capability
};
```

(String tetap non-owning: built-in → literal; custom → `std::string` milik
`PappPackage` yang outlive entry, sesuai kontrak lifetime sekarang.)

### 5. Launcher + gating

- **Launcher** (`AppListScreen`) menggambar **icon** + name + (opsional) badge
  runtime/origin. Icon = 1-bit pixelete via Aether (Plan 52). Grup per `category`.
- **Gating saat launch** (`AppRegistry::launch(id)`), tiga gerbang:
  1. **API**: `manifest.api_version.major == host.apiVersion.major` (Plan 48). Beda
     major → tolak ("app butuh API vX").
  2. **Capability** (Plan 42): tiap `needs[]` → `rt.capabilities().has(cap)`. Kurang
     → tolak, sebut cap yang hilang (`hintFor`).
  3. **Display-server** (Plan 51): `display_server == "aether"` → butuh AetherServer
     ter-mount. Tak ada → **bagian UI** ditolak; bila `mode == hybrid` atau
     `headless`, **bagian headless tetap jalan** di shell (Plan 54).
- Lulus tiga gerbang → spawn via `AppHostManager` (thread app, pause/resume gratis,
  Plan 22) untuk tier runtime sesuai `runtime`.

---

## Fase

| Fase | Isi | Tes |
|---|---|---|
| **0. Skema + builder** | `manifest.json` schema + tipe TS; `papp-build` (rename `nema-build`) keluarkan PAPP1 single-file & bundle; `papp/1` validator (mode×server) | host: build template → `.papp`; snapshot TOC; validator tolak `ui+headless` |
| **1. Parser PAPP1 device** | `PappPackage::parse(bytes,len)` (TOC, RLE per-entry, manifest entry-0); `installPapp` (ganti `installKapp`) | `papp_test` host: parse single-file + bundle; manifest+icon+assets ter-ekstrak; corrupt → tolak |
| **2. Registry diperluas** | `AppManifest` + `RuntimeTier` (reuse Plan 56) + enum `DisplayTarget/AppMode/ApiVersion`; launcher baca metadata | host: registry simpan/ambil semua field; list() utuh |
| **3. Gating launch** | tiga gerbang (api/cap/server) di `AppRegistry::launch`; headless-tetap-jalan saat server absen | sim: app `needs:[net.wifi]` di board tanpa wifi → tolak; `ui:aether` tanpa server → tolak UI, headless jalan |
| **4. Persistence LittleFS** | explode ke `/flash/apps/<id>/`; boot scan; OTA `installPapp` tulis disk (gabung Plan 38 Fase 2) | device/sim: install → reboot → app **masih ada** + icon termuat |
| **5. Transfer PLP** | `ExtOp::AppInstall` = PAPP1; (opsional) `AppInstallBegin/Chunk/End` streaming ke LittleFS | Forge→device: push `.papp` bundle (dgn icon) → muncul + launch |
| **6. Launcher icon + kategori** | render icon 1-bit, grup `category`, badge runtime | sim: launcher tampil icon per-app; grup benar |

Fase 0–3 tanpa hardware (host+WASM). 4–5 verifikasi device (build-only bila absen).

## File yang disentuh

| File | Aksi |
|---|---|
| `packages/nema-app-sdk/bin/nema-build.ts` | → `papp-build.ts`: output PAPP1 (single-file + bundle/TOC), pack icon/assets |
| `packages/nema-app-sdk/templates/*/kapp.json` | → `manifest.json` (+`runtime`,`mode`,`display_server`,`api_version`,`icon`,`category`) |
| `packages/nema-app-sdk/src/types.ts` | tipe `PappManifest` + enum runtime/mode/server |
| `firmware/core/include/nema/app/app_manifest.h` | perluas `AppManifest` + reuse `RuntimeTier` (Plan 56) + enum `DisplayTarget/AppMode/ApiVersion` |
| `firmware/core/include/nema/app/app_registry.h` / `.cpp` | simpan metadata penuh; `launch()` = tiga gerbang gating |
| `firmware/core/include/nema/apps/papp_package.h` + `src/apps/papp_package.cpp` | **baru** — parser PAPP1 (TOC+RLE), `PappPackage` |
| `firmware/core/src/apps/js_app_store.cpp` | `installKapp` → `installPapp`; pakai `PappPackage` (ganti `jsonField` ad-hoc) |
| `firmware/core/include/nema/apps/embedded_apps.h` | generator sertakan `runtime`,`mode`,`server`,`category` per embedded app |
| `firmware/core/include/nema/services/remote_service.h` | komentar `ExtOp::AppInstall` = PAPP1; (opsional) opcode streaming |
| `firmware/core/.../js_app_store.{h}` + Plan 38 `loadPersisted` | explode/scan `/flash/apps/<id>/` lewat `IFileSystem` |
| `firmware/core/.../app_list_screen.*` (launcher) | render icon + kategori + gating-aware (disable bila tak bisa launch) |
| `firmware/tests/papp_test.cpp` | **baru** — parse/validasi PAPP1 (single-file + bundle, corrupt, RLE) |

## Test

- **Builder (host)**: template → `.papp`; single-file & bundle; snapshot TOC;
  validator menolak `mode:ui`+`display_server:headless`, `schema` major mismatch.
- **Parser `papp_test` (host)**: round-trip pack→parse; manifest entry-0 dibaca
  tanpa load code; icon+assets ter-ekstrak; RLE per-entry; magic salah / length
  overflow / TOC korup → ditolak rapi (tak crash).
- **Registry (host)**: simpan/baca semua field; `list()` utuh; re-install id =
  replace.
- **Gating (sim/WASM)**: `needs:[net.wifi]` di board tanpa wifi → tolak (sebut cap);
  `display_server:aether` tanpa server → UI ditolak, `hybrid`/`headless` tetap
  jalan; `api_version` major beda → tolak.
- **Persistence (device/sim)**: install OTA → reboot → app + icon **masih ada**;
  LittleFS absen → volatile (tak crash).
- **Transfer (Forge→device)**: push bundle `.papp` ber-icon via PLP → muncul +
  launch; paket > MTU ter-fragment & reassembly benar.

## Risiko & mitigasi

| Risiko | Mitigasi |
|---|---|
| TOC custom = format baru yang harus dimaintain dua sisi (TS pack ↔ C++ parse) | Ikuti pola KAPP1/`klp_codec`: **test vector bersama** (sama seperti Plan 35) untuk jamin pack↔parse byte-exact. |
| Paket besar (assets) tak muat di RAM saat reassembly PLP | Opcode streaming `AppInstallBegin/Chunk/End` → tulis blob langsung ke LittleFS; v1 batasi ukuran reassembly. |
| Manifest JSON parser di device (sekarang `jsonField` ad-hoc, rapuh) | `PappPackage` pakai parser JSON kecil yg sama dgn sistem (atau subset terbatas terdokumentasi); validasi field wajib saat install. |
| `runtime: c` tak punya artifact yang dikirim (built-in only) | C = `AppKind::BuiltIn`, tak pernah lewat `.papp`/transfer; manifest-nya cuma metadata embedded. Hanya js/wasm yang punya container kirim. |
| Capability/`needs` salah-eja diam-diam lolos (bug AkiraOS) | Validasi `needs[]` vs katalog Plan 42 saat install; cap tak dikenal → **tolak** (bukan abaikan), tanpa wildcard. |
| Drift versi: app lama vs API host baru | `api_version` major-match gate saat launch; minor backward-compat. IDL Plan 48 = SSOT versi. |
| Single-file vs bundle ambiguitas parse | Magic `PAPP1\n` sama; deteksi bentuk dari struktur setelah magic (2-newline body vs `u16 entryCount`+TOC). `papp-build` menandai eksplisit (single-file = entryCount=1 implisit / mode `--script`). |
