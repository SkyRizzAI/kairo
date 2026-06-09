# 38 — Storage / Filesystem HAL + App Store (LittleFS + microSD)

> **Filesystem layer** untuk **persistensi** & file besar: simpan app `.kapp` yang
> ter-install supaya **tahan reboot**, data per-app yang ter-sandbox, file user, dan
> nanti dasar **firmware-OTA staging** (Plan 39). Sekarang Kairo cuma punya
> `IConfigStore` (NVS: key-value, namespace ≤15 char, value ~4KB) — tak cukup untuk
> menyimpan bundle app. Plan ini memberi **`IFileSystem`** + backend (LittleFS
> internal flash, microSD) + **persistence layer untuk `JsAppStore`** + **per-app
> storage sandbox + quota**.
>
> Asal-usul: rekomendasi **P4** di `docs/research/akiraos-vs-kairo.md` (pola
> `fs_manager` AkiraOS: SD→internal→RAM fallback + isolasi storage per-app).

> ⚠️ **KOREKSI SCOPE (2026-06-09, setelah Plan 37 maju ke Fase 6/8):**
> Plan 38 **BUKAN lagi blocker untuk OTA install app.** Plan 37 Fase 6 sudah **✅
> selesai *filesystem-free*** — `JsAppStore::installKapp()` mendaftarkan app yang
> di-push live di RAM, langsung muncul di Apps (**volatile**, hilang saat reboot).
> Yang masih dibutuhkan Plan 38 cuma:
> 1. **Persistensi**: app ter-install **tahan reboot** (tulis `.kapp` ke flash-FS,
>    di-reload saat boot). → butuh **flash-FS internal (SPIFFS/LittleFS)**, **BUKAN
>    microSD**.
> 2. **microSD** = hanya untuk **bulk/removable libraries** (Fase 7 Plan 37), bukan
>    syarat persistensi.
> 3. **Storage file umum** (data per-app besar, file user) + dasar **firmware-OTA
>    staging** (Plan 39).

- Status: 📝 **PLANNED** (2026-06-09). Belum mulai.
- Milestone: M10 (Custom App Ecosystem) — pelengkap (persistensi) Plan 37.
- Depends on: **24 (Config Store)**, **16 (ESP32 Platform)**, **28 (SkyRizz E32 —
  microSD di SPI3)**, **05 (Service Container)**.
- Enables (bukan blocks): **persistensi app `.kapp` Plan 37**, **37 Fase 7
  (microSD)**, **39 (Firmware OTA — staging image)**.

> Catatan keselarasan: Plan 37 dikerjakan paralel (sudah Fase 6/8). Plan ini
> **tidak menyentuh** runtime JS/QuickJS/bridge/`JsAppStore` Plan 37 — ia cuma
> menyediakan `IFileSystem` + lapisan **persistensi** tipis yang dipanggil
> `JsAppStore` (load `.kapp` dari `/flash/apps` saat boot, simpan saat
> `installKapp`). Titik temu = `JsAppStore` baca/tulis lewat `IFileSystem`. Aman
> dikerjakan berbarengan.

---

## 0. Keputusan kunci & alasan

### 0.1 LittleFS (bukan SPIFFS) di internal flash
| Opsi | Verdict |
|---|---|
| **LittleFS** | ✅ power-fail safe (copy-on-write + journaling), wear-leveling, direktori beneran, cepat. ESP-IDF: komponen `esp_littlefs` / `joltwallet/littlefs`. |
| SPIFFS | ⚠️ flat namespace (no real dir), tak power-fail safe, lambat saat penuh. Partisi `spiffs` yang ada bisa **dipakai-ulang** subtype-nya untuk LittleFS. |
| FAT (internal) | boros, butuh wear-leveling layer terpisah |

> Partisi `spiffs` di `partitions.csv` skyrizz sudah **~11 MB** (0x510000, 0xAE0000).
> Cukup besar untuk ratusan `.kapp`. Plan ini me-mount partisi itu sebagai LittleFS
> (ganti subtype/`littlefs` atau pakai partisi data generik). microSD = FAT.

### 0.2 Satu interface, banyak backend (pola Kairo: capability, bukan board)
`IFileSystem` adalah HAL (di `hal/`), bukan board-spesifik. Backend dipilih saat
runtime via capability/registrasi platform — **bukan** `#ifdef board`. Mengikuti
prinsip `CLAUDE.md` ("check capabilities, never board type") dan pola `fs_manager`
AkiraOS yang **graceful fallback** (SD → internal → RAM).

### 0.3 Mount table + sandbox path per-app
Path absolut dengan prefix mount: `/flash/...`, `/sd/...`, `/ram/...`. App JS (Plan
37) **tidak** dapat path absolut bebas — `kairo.storage` & `IAppStore` memetakan ke
`/flash/apps/<app-id>/...`. App tak bisa keluar dari sandbox-nya (path traversal
ditolak). Tambah **quota** byte per app.

---

## 1. Goal (acceptance tingkat-tinggi)

1. **`IFileSystem` HAL** portabel: open/read/write/close, mkdir, list(dir), stat,
   remove, rename. Teruji di **host + WASM + esp32**.
2. **Backend**: `LittleFsBackend` (internal flash, esp32), `SdFatBackend` (microSD
   SPI3 skyrizz), `HostFsBackend` (POSIX temp dir, untuk host test), `MemFsBackend`
   (in-memory; WASM + fallback). Graceful fallback bila SD absen.
3. **Mount table**: `/flash` (LittleFS), `/sd` (FAT, opsional), `/ram` (mem). Platform
   mendaftarkan mount yang tersedia; capability `storage.fs`, `storage.sd`.
4. **Persistensi `JsAppStore`**: saat boot, `loadEmbeddedJsApps` + scan
   `/flash/apps/*.kapp` → `JsAppStore::registerApp`; saat `installKapp` (OTA),
   tulis juga `.kapp` ke `/flash/apps/<id>.kapp`. → app ter-install **tahan reboot**
   (tanpa ini, install Plan 37 cuma volatile). microSD opsional (bulk libraries).
5. **Per-app sandbox + quota**: `kairo.storage` (Plan 37) routing ke
   `/flash/apps/<id>/`; tolak path di luar sandbox; batasi byte per app.
6. Semua teruji **host + WASM dulu**, baru device (build-only kalau device tak ada).

---

## 2. Arsitektur

### 2.1 HAL (`firmware/core/include/kairo/hal/filesystem.h`)
```cpp
struct FileStat { uint32_t size; bool isDir; uint32_t mtime; };

struct IFileSystem : IDriver {
    DriverKind kind() const override { return DriverKind::Storage; }
    // returns 0 / negative errno
    virtual int   open (const char* path, const char* mode, int& fd) = 0;
    virtual int   read (int fd, void* buf, size_t n, size_t& out) = 0;
    virtual int   write(int fd, const void* buf, size_t n, size_t& out) = 0;
    virtual int   close(int fd) = 0;
    virtual int   stat (const char* path, FileStat& out) = 0;
    virtual int   mkdir(const char* path) = 0;
    virtual int   remove(const char* path) = 0;
    virtual int   rename(const char* from, const char* to) = 0;
    using ListFn = void(*)(void* user, const char* name, const FileStat&);
    virtual int   list (const char* path, ListFn fn, void* user) = 0;
    virtual bool  available() const = 0;     // mounted & writable?
    virtual uint64_t freeBytes() const = 0;
};
```
Plus helper non-virtual (`readAll(path)→vector`, `writeAll(path,bytes)`) di header
biar call-site ringkas (pola sama `RemoteScreenTap`/transport kita).

### 2.2 Backend
```
firmware/platforms/esp32/.../esp32_littlefs.{h,cpp}   // esp_littlefs di partisi data
firmware/platforms/esp32/.../esp32_sd.{h,cpp}         // sdspi (SPI3 skyrizz) + FATFS
firmware/platforms/host/.../host_fs.{h,cpp}           // POSIX, root = $TMPDIR/kairo
firmware/platforms/wasm/.../mem_fs.{h,cpp}            // in-memory (juga fallback)
firmware/core/src/hal/mount_table.{h,cpp}             // resolve "/flash/.." → backend
```
- esp32 platform `registerDrivers`: mount LittleFS → `/flash`; coba SD → `/sd`
  (kalau gagal, capability `storage.sd` tak di-add — app cek `device.has("storage.sd")`).
- WASM/host: MemFs/HostFs → `/flash` (biar Plan 37 jalan di sim tanpa hardware).

### 2.3 Persistensi `JsAppStore` (bukan IAppStore baru)
Plan 37 **sudah** punya `JsAppStore` (RAM registry, `firmware/core/include/kairo/
plugins/js_app_store.h`) dengan `registerApp()` + `installKapp()`. Plan 38 cuma
menambah **persistensi** lewat `IFileSystem` — tak perlu interface store baru:
```
// di JsAppStore (extend):
void loadPersisted(Runtime& rt);   // boot: scan /flash/apps/*.kapp → registerApp
bool installKapp(...);             // OTA: registerApp + writeAll(/flash/apps/<id>.kapp)
bool removePersisted(id);          // hapus file + unregister
```
- `.kapp` disimpan utuh di `/flash/apps/<id>.kapp`.
- Boot: `loadEmbeddedJsApps` (built-in) **lalu** `JsAppStore::loadPersisted` (yang
  OTA-installed) → semua muncul di Apps. Volatile-install (sekarang) tetap jalan;
  kalau `IFileSystem` tersedia, install jadi persisten otomatis.
- microSD: scan `/sd/apps/*.kapp` juga (Fase 7) — sumber yang sama, mount beda.

### 2.4 Sandbox & quota
- `kairo.storage` (Plan 37 host-function) → namespace `/flash/apps/<id>/data/`.
- `AppFsSandbox` wrapper: normalisasi path, tolak `..`/absolut, prefix paksa,
  cek quota (`usedBytes(appDir) + n <= quotaBytes`).

---

## 3. Fase implementasi (tiap fase build & teruji)

| Fase | Isi | Tes |
|---|---|---|
| **0. HAL + MemFs/HostFs** | `IFileSystem`, `MemFsBackend`, `HostFsBackend`, mount table | `fs_test` host: write→read→list→stat→remove; path-traversal ditolak |
| **1. LittleFS esp32** | `esp_littlefs` mount partisi data → `/flash`; capability `storage.fs` | esp32 build hijau; (device) tulis file survive reboot |
| **2. Persistensi JsAppStore** | `JsAppStore::loadPersisted` (boot scan) + tulis `.kapp` saat `installKapp`; pakai `IFileSystem` | sim+device: install app via OTA (Plan 37) → reboot → app **masih ada** & launch |
| **3. Sandbox + quota** | `AppFsSandbox`; `kairo.storage` route per-app; quota byte | sim: app A tak bisa baca dir app B; tulis > quota ditolak |
| **4. microSD (skyrizz)** | `SdFatBackend` SPI3 + FATFS → `/sd`; `SdAppStore`; fallback bila absen | device: app load dari `/sd/apps`; cabut SD → degrade aman |
| **5. WASM persistence (opsional)** | MemFs → IndexedDB di Forge sim biar app install persist di browser | Forge: install app di sim, reload, app masih ada |

Fase 0,3,5 tanpa hardware. 1,2,4 verifikasi device (build-only bila device absen).

---

## 4. Acceptance criteria (definition of done)
- [ ] `IFileSystem` + MemFs/HostFs jalan; `fs_test` host PASS (incl. path-traversal reject).
- [ ] LittleFS ter-mount di esp32 (`/flash`); file survive reboot; build skyrizz hijau.
- [ ] `JsAppStore` persisten: app OTA-installed (Plan 37 Fase 6) **tahan reboot**
      (tulis ke `/flash/apps`, di-reload saat boot).
- [ ] Sandbox per-app + quota di-enforce (app tak bisa keluar dir-nya / lampaui quota).
- [ ] microSD backend jalan di skyrizz + fallback aman saat SD tak ada (bulk libraries).
- [ ] WASM/host pakai backend in-memory → Plan 37 tetap jalan di sim tanpa hardware.

---

## 5. Memori & risiko
| Item | Catatan |
|---|---|
| Partisi | `spiffs` 11MB sudah ada → mount LittleFS di situ (atau ubah subtype ke `littlefs`). Tak perlu mengecilkan app partition. |
| Power-fail | LittleFS COW aman; SPIFFS tidak → pilih LittleFS. |
| SD + console | SD pakai SPI3 (skyrizz), tak bentrok USB-Serial-JTAG. |
| WASM | Tak ada flash → MemFs (atau IndexedDB) supaya Plan 37 tetap teruji di Forge sim. |
| Quota | Hitung ukuran dir on-demand (cache) biar `list` tak mahal. |

## 6. Non-goals (v1)
- Bukan POSIX penuh (no symlink/permissions Unix). Cukup file+dir+CRUD.
- Tidak ada enkripsi file (rahasia → settings terenkripsi di Plan 39).
- Tidak ada FUSE/USB-MSC mount ke PC (itu Plan 34 §7b USB-MSC, terpisah).
