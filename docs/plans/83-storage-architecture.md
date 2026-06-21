# Plan 83 — Storage Architecture: AppStorage, VFS Restructure & Storage Management

## Goals

1. **VFS restructure** — clean path hierarchy: `system/assets/`, `data/<bundle-id>/`, `sd/`
2. **AppStorage API** — namespaced file storage for apps; `ctx.storage()` in `ProcessContext`
3. **StorageService** — routing (internal ↔ SD), move operations, usage stats
4. **AppManifest storage fields** — `storageMovable`, `hasCriticalData`
5. **Built-in app migration** — BadUSB, DolphinApp, desktop wallpaper → new paths
6. **NVS namespace fix** — bundle-ID hash (15-char NVS limit bug, silently broken on HW)
7. **JS SDK** — `nema.storage.readFile/writeFile/listFiles/removeFile/bytesUsed()`
8. **Storage Settings screen** — volume overview + per-app list + move-to-SD/internal

---

## Background

### State sebelum plan ini

- App mendapat `rt_.fs()` langsung → tidak ada isolation, app A bisa baca/tulis data app B
- VFS paths tidak konsisten: `/badusb/` di root, `/anims/` di root, `/data/` flat tanpa namespace
- JS `nema.storage.get/set()` wraps KV config store dengan `appId_` sebagai namespace →
  **silent bug**: NVS hard-limit namespace = 15 char, bundle ID seperti `"com.palanu.badusb"`
  (18 char) truncate/fail di ESP32 hardware
- Tidak ada SD-card routing layer — code harus hard-code `/sd/apps/` sendiri
- Tidak ada storage management UI (ukuran per app, move to SD)

### Desain decisions

| Keputusan | Alasan |
|---|---|
| `ctx.storage()` di `ProcessContext`, bukan `Runtime` | App tidak boleh akses global storage; isolation by API |
| `rt_.fs()` tetap ada untuk system code | `aether`, screens, services memang butuh full VFS |
| `file_browser_screen` tetap pakai `rt_.fs()` | File manager by design butuh akses penuh |
| Routing config di `IConfigStore` (existing) | Tidak perlu storage baru untuk menyimpan "where does app X live" |
| NVS namespace = djb2 hash 8-char hex | Deterministic, always fits, tidak breaking existing data (hash baru) |
| SD data path mirrors internal | `/data/<id>/` internal ↔ `/sd/data/<id>/` external — konsisten, predictable |

---

## VFS Layout (target)

### Internal flash (`/`, LittleFS)

```
/system/
  /assets/
    /anims/          ← .panim wallpaper bawaan firmware
    /icons/          ← future: custom icon packs
    /sounds/         ← future: notification sounds
/apps/               ← .papp bundles (scan target)
/data/
  /com.palanu.badusb/   ← BadUSB scripts (dulu /badusb/)
  /com.palanu.clock/    ← app data, per-bundle-id
  /com.user.myapp/
/update/             ← OTA staging
```

### SD card (`/sd/`, FAT32, optional)

```
/sd/
  /apps/             ← .papp bundles (scan target, merged dengan /apps/)
  /assets/
    /anims/          ← extra wallpapers dari SD
    /themes/         ← future: theme packs
  /data/
    /com.palanu.badusb/  ← jika user pindahkan ke SD
    /com.user.myapp/
  /captures/         ← kamera
  /music/            ← audio playback
```

**Prinsip:**
- `system/` = firmware owns, app tidak write ke sini
- `data/<id>/` = per-app isolated, tidak bisa collision antar app
- File browser intentionally mendapat akses full VFS (bukan AppStorage)
- SD = expansion, tidak wajib; semua fitur core jalan tanpa SD

---

## API Design

### AppStorage (C++)

```cpp
// firmware/core/include/nema/fs/app_storage.h

enum class StorageLocation { Internal, External, Auto };

class AppStorage {
public:
    // File ops — auto-routed ke /data/<bundle-id>/ atau /sd/data/<bundle-id>/
    bool     write (const char* name, const uint8_t* data, size_t len);
    bool     read  (const char* name, std::vector<uint8_t>& out);
    bool     remove(const char* name);
    bool     exists(const char* name) const;
    std::vector<std::string> list();

    // Critical sub-storage — SELALU internal, immune dari user move
    AppStorage critical();

    // Info
    size_t         usedBytes()  const;
    StorageLocation location()  const;

    // Internal use — created by StorageService
    AppStorage(std::string bundleId, IFileSystem& vfs,
               IConfigStore& cfg, bool criticalScope = false);
private:
    std::string     bundleId_;
    IFileSystem&    vfs_;
    IConfigStore&   cfg_;
    bool            critical_;

    std::string resolvePath(const char* name) const;
};
```

Tambahan di `ProcessContext`:

```cpp
// proc/process_context.h
AppStorage storage();           // scoped ke bundle-id app ini, respects routing
AppStorage criticalStorage();   // shorthand: selalu internal
```

`AppHost` inject `bundleId_` ke `ProcessContext` saat launch:

```cpp
// app_host.cpp launchThread():
ctx_.setBundleId(app_.id());
```

### StorageService

```cpp
// firmware/core/include/nema/services/storage_service.h

class StorageService : public IService {
public:
    // Dipanggil oleh AppStorage::resolvePath()
    std::string resolveDataPath(const char* bundleId,
                                const char* name,
                                bool critical = false);

    // Management — untuk StorageSettingsScreen
    struct AppStorageInfo {
        std::string bundleId;
        std::string displayName;
        StorageLocation location;
        size_t      internalBytes;
        size_t      externalBytes;
        bool        movable;       // dari AppManifest::storageMovable
        bool        hasCritical;   // dari AppManifest::hasCriticalData
    };
    std::vector<AppStorageInfo> allApps(AppRegistry& apps);
    bool move(const char* bundleId, StorageLocation to, IFileSystem& vfs);

    // Volume info
    VolumeInfo internal();   // { totalBytes, usedBytes, freeBytes }
    VolumeInfo external();   // { 0,0,0 } jika tidak ada SD
    bool       hasExternal();

    // IService
    const char* name() const override { return "StorageService"; }
    void start() override {}
    void stop()  override {}
    void tick(uint64_t) override {}
};
```

**Routing logic di `resolveDataPath()`:**

```
1. critical=true  → selalu /data/<id>/<name>
2. Baca config "stor" / nsKey(bundleId) → "ext" | "int" | (tidak ada)
3. "ext" dan SD mounted → /sd/data/<id>/<name>
4. Lainnya → /data/<id>/<name>
```

**NVS namespace key** (15-char limit):

```cpp
static std::string nsKey(const std::string& id) {
    uint32_t h = 5381;
    for (char c : id) h = ((h << 5) + h) + c;
    char buf[9];
    snprintf(buf, sizeof(buf), "%08x", h);
    return buf;  // "a3f2c891" — 8 chars, always fits
}
// Config: namespace="stor", key=nsKey(bundleId), value="int"|"ext"
```

### AppManifest (tambahan)

```cpp
// app/app_manifest.h
struct AppManifest {
    // ... existing fields ...
    bool storageMovable   = true;   // user bisa pindah data ke SD?
    bool hasCriticalData  = false;  // punya data yang harus tetap internal?
};
```

### JS SDK

```typescript
// packages/app-sdk IDL (nema:storage/fs)

// KV — tetap, tidak breaking (backed by IConfigStore)
nema.storage.get(key: string): string | null
nema.storage.set(key: string, value: string): void
nema.storage.getInt(key: string): number | null
nema.storage.setInt(key: string, value: number): void
nema.storage.remove(key: string): boolean

// File — baru (backed by AppStorage, namespaced)
nema.storage.readFile(name: string): string | null        // UTF-8
nema.storage.writeFile(name: string, data: string): boolean
nema.storage.listFiles(): string[]
nema.storage.removeFile(name: string): boolean
nema.storage.bytesUsed(): number
```

Implementasi di `nema_host_impl.cpp`:

```cpp
// nema:storage/fs
std::optional<std::string> fs_read(std::string_view name) override {
    auto stor = storageService_.openFor(appId_, vfs_, cfg_);
    std::vector<uint8_t> buf;
    if (!stor.read(std::string(name).c_str(), buf)) return std::nullopt;
    return std::string(buf.begin(), buf.end());
}
bool fs_write(std::string_view name, std::string_view data) override {
    auto stor = storageService_.openFor(appId_, vfs_, cfg_);
    return stor.write(std::string(name).c_str(),
                      (const uint8_t*)data.data(), data.size());
}
// ... dst
```

**Fix NVS namespace** di `kv_get/set` yang ada:

```cpp
// Sebelum:
rt_.config().getString(appId_.c_str(), key, v)
// Sesudah:
rt_.config().getString(nsKey(appId_).c_str(), key, v)
```

---

## Asset Search Paths (untuk aether scan anims)

Tidak butuh union mount. Cukup scan dua direktori, merge hasilnya:

```cpp
// shell/desktop_livewall.cpp atau asset helper
static const char* kAnimSearchPaths[] = {
    "/system/assets/anims",
    "/sd/assets/anims",
    nullptr
};

std::vector<std::string> listAnimPaths(IFileSystem& vfs) {
    std::vector<std::string> result;
    for (const char** p = kAnimSearchPaths; *p; ++p) {
        std::vector<FsEntry> entries;
        if (!vfs.list(*p, entries)) continue;
        for (auto& e : entries)
            if (!e.isDir) result.push_back(std::string(*p) + "/" + e.name);
    }
    return result;
}
```

---

## Migration Map (existing code)

| File | Saat ini | Setelah plan 83 |
|---|---|---|
| `bad_usb_app.cpp` | `rt_.fs()` → `/badusb/` | `ctx.storage().read/list()` → `/data/com.palanu.badusb/` |
| `dolphin_app.cpp` | `ctx.runtime().fs()` → `anims/laptop.panim` | path → `system/assets/anims/laptop.panim` |
| `dolphin_demo.cpp` | `rt_.fs()` → anim paths | path update sama |
| `desktop_livewall.cpp` | `rt_.fs()` → `"anims/laptop.panim"` | `"system/assets/anims/laptop.panim"` + scan helper |
| `dolphin_anim.cpp` | `DOLPHIN_ENTRIES` path `"anims/..."` | `"system/assets/anims/..."` |
| `wasm_platform.cpp` | seed `/anims/laptop.panim` | seed `/system/assets/anims/laptop.panim` |
| `esp32_platform.cpp` | mkdir `/apps`, `/data`, `/badusb` | mkdir `/system/assets/anims`, `/apps`, `/data`; migrate `/badusb/` → `/data/com.palanu.badusb/` |
| `nema_host_impl.cpp` | `appId_.c_str()` langsung ke NVS | `nsKey(appId_)` untuk KV namespace |
| `file_browser_screen.cpp` | `rt_.fs()` langsung | **tidak berubah** — file manager by design |
| Settings screens | `rt_.config()` | **tidak berubah** — sudah benar |

### One-time boot migration

Di `esp32_platform.cpp` dan `wasm_platform.cpp`, setelah VFS ready:

```cpp
// Migrate /badusb/ → /data/com.palanu.badusb/ (sekali saja)
if (vfs_.exists("/badusb") && !vfs_.exists("/data/com.palanu.badusb")) {
    rt.log().info("Platform", "migrating /badusb/ → /data/com.palanu.badusb/");
    vfs_.rename("/badusb", "/data/com.palanu.badusb");
}
```

---

## Storage Settings Screen

```
Settings → Storage

  Internal Flash   [████████░░░░░░░░]   28 KB / 512 KB
  SD Card          [██░░░░░░░░░░░░░░]  2.2 GB / 8 GB
  ─ (No SD Card) ─                     (jika tidak ada)

  Apps ──────────────────────────────────────────────
  BadUSB           2 KB   Internal   [Move to SD →]
  Clock            1 KB   Internal   [Move to SD →]
  My Custom App  120 MB   SD Card    [← Move to Int]
  JS Engine        0 KB   Internal
```

- Move = async: copy semua file → update routing config → delete source
- Apps dengan `storageMovable = false` tidak tampil tombol Move
- Apps dengan `hasCriticalData = true` tampil note "⚠ some data stays internal"
- Scan ukuran on open (bukan live — terlalu mahal di embedded)

---

## Implementation Phases

### Fase 1 — Path restructure + NVS fix (tidak breaking user data)

- [ ] Fix NVS namespace: `nsKey(bundleId)` di `nema_host_impl.cpp`
- [ ] Rename embedded seed: `/anims/` → `/system/assets/anims/` di `wasm_platform.cpp`
- [ ] Update `dolphin_anim.cpp` `DOLPHIN_ENTRIES` paths
- [ ] Update `desktop_livewall.cpp` default path + scan helper
- [ ] Update `dolphin_app.cpp` + `dolphin_demo.cpp` paths
- [ ] Update `esp32_platform.cpp` mkdir + one-time migration `/badusb/` → `/data/com.palanu.badusb/`
- [ ] Update `wasm_platform.cpp` mkdir structure
- [ ] Build green: host + wasm

### Fase 2 — AppStorage + StorageService core

- [ ] `firmware/core/include/nema/fs/app_storage.h`
- [ ] `firmware/core/src/fs/app_storage.cpp`
- [ ] `firmware/core/include/nema/services/storage_service.h`
- [ ] `firmware/core/src/services/storage_service.cpp`
- [ ] Tambah `bundleId_` + `storage()` ke `ProcessContext`
- [ ] `AppHost::launchThread()` inject `bundleId`
- [ ] Tambah `storageMovable` + `hasCriticalData` ke `AppManifest`
- [ ] Register `StorageService` di platform init (esp32 + wasm)
- [ ] Build green: host + wasm

### Fase 3 — Built-in app migration

- [ ] `bad_usb_app.cpp` → `ctx.storage().list()` + `ctx.storage().read()`
- [ ] `dolphin_app.cpp` → path sudah fixed di Fase 1; confirm via `ctx.runtime().fs()` atau biarkan karena system code
- [ ] Tambah `AppManifest` storage fields ke semua built-in apps
- [ ] Build green + verify BadUSB scripts masih terbaca

### Fase 4 — JS SDK file storage

- [ ] Tambah IDL entries: `nema:storage/fs` (`readFile`, `writeFile`, `listFiles`, `removeFile`, `bytesUsed`)
- [ ] Implementasi di `nema_host_impl.cpp`
- [ ] Regenerate `nema_api.gen.h`
- [ ] Update embedded runtime JS (`packages/app-sdk`) expose `nema.storage.readFile` dll.
- [ ] Test via embedded JS sysinfo app

### Fase 5 — Storage Settings Screen

- [ ] `firmware/core/src/screens/storage_settings_screen.cpp`
- [ ] `firmware/core/include/nema/screens/storage_settings_screen.h`
- [ ] Tambah entry "Storage" di Settings menu
- [ ] Test: tampil volume info, per-app list, move button (WASM)

---

## Acceptance Criteria

- BadUSB scripts tetap terbaca setelah update (migration otomatis)
- `ctx.storage().write("x.bin")` di app → file muncul di `/data/<id>/x.bin`
- JS `nema.storage.writeFile("data.json", "...")` → persists dan terbaca ulang
- `StorageService::move("com.palanu.badusb", External)` → file pindah ke `/sd/data/com.palanu.badusb/`
- NVS kv_get/set tidak lagi truncate bundle ID
- Storage Settings screen tampil volume bar + app list
- Build green: host + wasm + esp32

---

## Depends on

- Plan 82 (asset architecture — VFS + .panim) — ✅ done
- Plan 19.6 (AppHost / AppContext / ProcessContext) — ✅ done

## Blocks

- Plan 76 (app-service-daemon: app data persistence) — butuh AppStorage
- Plan 38 (storage filesystem HAL maturation) — superset dari plan ini, merujuk ke sini
