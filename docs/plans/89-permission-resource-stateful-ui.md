# 89 — Permission Grant Flow, Resource Lease & Stateful Screen System

> **Lanjutan Plan 87 (app capability model) + Plan 86 (process-first app).** Plan 87
> membangun PermissionService + PermissionScreen + AppDetailScreen — infrastruktur
> sudah ada tapi **belum terhubung ke WASM runtime** dan **resource lease belum ada**.
> Plan ini menutup gap yang ditemukan selama testing WiFi Marauder: permission dialog
> tidak muncul, chip WiFi tidak dilepas ke app, dan storage screen stale.

- Status: ✅ Done
- Milestone: M10 (App Platform maturity)
- Depends on: **87** (permission infra), **86** (process model), **88** (remote v2)
- Target: skyrizz-e32 (ESP32-S3) + WASM simulator

---

## 0. Konteks — gap yang ditemukan

Selama testing WiFi Marauder WASM di device nyata, ditemukan 4 masalah:

| # | Masalah | Root cause |
|---|---|---|
| G1 | Dialog permission tidak muncul saat Marauder dibuka | `wasm_wifi.cpp` langsung panggil `IRadioWifi` tanpa lewat `PermissionService::request()` |
| G2 | Settings → Apps → WiFi Marauder → "No permissions requested" | `status()` selalu 0 karena `request()` tidak pernah dipanggil |
| G3 | Chip WiFi tidak dikelola saat Marauder ambil alih (`wifi_monitor_open`) | Tidak ada resource lease / service pause mechanism |
| G4 | Storage screen menampilkan "0 B" sampai tombol ditekan | Layar stale karena `onResume()` tidak set `dirty_ = true` (sudah difix di Plan 89 commit awal) |

G4 sudah difix. Plan ini menyelesaikan G1, G2, G3 — dan membangun fondasi yang benar
agar app lain di masa depan bisa minta resource sensitif dengan aman.

---

## 1. Fase 1 — Permission wire-up di WASM runtime

### 1.1 Masalah

`PermissionService::request(appId, cap)` sudah ada dan sudah terhubung ke GUI
(`guiTick()` dipanggil setiap frame oleh `GuiService` → push `PermissionScreen` saat
ada `pending_`). Tapi tidak ada yang memanggil `request()` sebelum operasi WiFi.

### 1.2 Solusi

Tambahkan helper `checkPerm(runtime, cap)` di `wasm_wifi.cpp` yang:
1. Ambil `appId` dari `WasmHostCtx` (sudah ada di context)
2. Panggil `PermissionService::request(appId, cap)` — ini **blocking** di app thread;
   GUI thread akan push dialog dan user memilih Allow/Deny
3. Return `true` jika granted (status == 1), `false` jika denied (status == 2)

Setiap binding WiFi yang sensitif memanggil `checkPerm` sebelum akses radio:

| Binding | Capability yang dicek |
|---|---|
| `wifi_scan` | `net.wifi.scan` |
| `wifi_monitor_open` | `net.wifi.monitor` |
| `wifi_inject` | `net.wifi.inject` |
| `wifi_deauth_start` | `net.wifi.inject` (destructive) |
| `wifi_beacon_spam_start` | `net.wifi.inject` |

`wifi_deauth_stop`, `wifi_monitor_close`, `wifi_beacon_spam_stop` tidak perlu cek —
hanya operasi stop, tidak berbahaya.

### 1.3 Implementasi

```cpp
// wasm_wifi.cpp — helper
static uint8_t checkPerm(IM3Runtime rt, const char* cap) {
    WasmHostCtx* h = hostOf(rt);
    if (!h || !h->ctx) return 2;  // deny if no context
    auto* perm = h->ctx->runtime().container().resolve<PermissionService>();
    if (!perm) return 1;  // no permission service = permissive (dev mode)
    return perm->request(h->ctx->bundleId(), cap);
}

// Contoh pakai di wasm_wifi_scan:
m3ApiRawFunction(wasm_wifi_scan) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, outOff);
    m3ApiGetArg(int32_t,  cap);
    if (checkPerm(runtime, "net.wifi.scan") != 1) m3ApiReturn(-1);
    // ... rest of impl
}
```

**Perlu diverifikasi**: `WasmHostCtx` memiliki `bundleId()` atau field yang equivalent.
Kalau belum ada, tambahkan di `WasmHostCtx` saat WASM app diload (ambil dari manifest).

### 1.4 Checklist Fase 1

- [x] Verifikasi `WasmHostCtx` punya akses ke `bundleId` / `appId`
- [x] Tambah `checkPerm()` helper di `wasm_wifi.cpp`
- [x] Guard `wifi_scan` dengan `net.wifi.scan`
- [x] Guard `wifi_monitor_open` dengan `net.wifi.monitor`
- [x] Guard `wifi_deauth_start`, `wifi_inject`, `wifi_beacon_spam_start` dengan `net.wifi.inject`
- [x] Test di simulator: permission dialog muncul pertama kali, tidak muncul lagi setelah grant
- [x] Test di device: Settings → Apps → WiFi Marauder menampilkan permission toggles setelah grant

---

## 2. Fase 2 — Resource lease system

### 2.1 Masalah

Ketika WiFi Marauder panggil `wifi_monitor_open()`, ESP32 switch ke promiscuous mode —
chip WiFi tidak lagi bisa dipakai untuk connected station mode. Tapi:
- `WifiService` (sistem) masih berjalan dan mungkin mencoba reconnect / DHCP refresh
- `NtpService` mungkin mencoba NTP sync
- mDNS / httpd (kalau aktif) masih subscribe ke WiFi events

Ini bisa menyebabkan konflik driver level.

### 2.2 Solusi: `ResourceBroker` + lease

Buat `ResourceBroker` sederhana di `CapabilityRegistry` atau sebagai service terpisah:

```
App                 ResourceBroker         WifiService
 |---lease(net.wifi.raw)-->|                   |
 |                         |---pause()-------->|
 |<---lease granted--------|                   |
 |   (uses raw WiFi)       |                   |
 |---release(lease)------->|                   |
 |                         |---resume()------->|
```

**Lease** = token yang app pegang selama pakai resource. Kalau app crash/exit, broker
otomatis release (RAII).

Scope Plan 89: implementasi minimal —
- `ILeasable` interface: `pause()` / `resume()`
- `WifiService` implements `ILeasable`
- `ResourceBroker` di `CapabilityRegistry` atau standalone service
- `wifi_monitor_open` di `wasm_wifi.cpp` acquire lease sebelum buka promiscuous
- `wifi_monitor_close` / app exit release lease

Scope masa depan: BT lease, multi-app conflict resolution, lease timeout.

### 2.3 Desain `ResourceBroker`

```cpp
// firmware/core/include/nema/system/resource_broker.h
class ResourceBroker {
public:
    // Token returned on successful lease.
    using LeaseId = uint32_t;
    static constexpr LeaseId INVALID = 0;

    // Acquire exclusive use of a resource. Calls pause() on registered holders.
    // Returns INVALID if already leased by another tenant.
    LeaseId acquire(const char* resource, const char* tenantId);

    // Release lease. Calls resume() on holders.
    void release(LeaseId id);

    // Register a service as a resource holder (called during service init).
    void registerHolder(const char* resource, ILeasable* holder);

private:
    struct Lease { std::string resource; std::string tenant; };
    std::map<LeaseId, Lease> leases_;
    std::multimap<std::string, ILeasable*> holders_;
    LeaseId nextId_ = 1;
    std::mutex mu_;
};
```

```cpp
// firmware/core/include/nema/system/leasable.h
class ILeasable {
public:
    virtual void pause()  = 0;
    virtual void resume() = 0;
    virtual ~ILeasable() = default;
};
```

`WifiService::pause()` → disconnect / stop reconnect loop, stop NTP trigger
`WifiService::resume()` → restart reconnect if was connected

### 2.4 Checklist Fase 2

- [x] Buat `ILeasable` interface (`firmware/core/include/nema/system/leasable.h`)
- [x] Buat `ResourceBroker` service (`firmware/core/include/nema/system/resource_broker.h`, `.cpp`)
- [x] `WifiService` implement `ILeasable` (pause/resume reconnect loop)
- [x] Register `ResourceBroker` di Runtime (atau inject via ServiceContainer)
- [x] `wifi_monitor_open` → acquire lease `"net.wifi.raw"`; `wifi_monitor_close` → release
- [x] Lease auto-release di `WasmHostCtx` destructor
- [x] Test: buka Monitor Mode → sistem WiFi pause → tutup Monitor → reconnect

---

## 3. Fase 3 — Stateful screen / reactive refresh

### 3.1 Masalah

Storage settings dan screen lain menampilkan data yang:
- Diload secara synchronous di `build()` (bisa lambat: scan dir, stat VFS)
- Tidak ada notifikasi kalau data berubah (e.g., app diinstall, SD card dicabut)

Sekarang: stale sampai button ditekan (karena `onAction()` set `dirty_ = true`).

### 3.2 Solusi: `requestRebuild()` dari background task

Pendekatan sederhana yang tidak butuh full reactive framework:

**Option A (recommended)**: Async data load + post event
- `onResume()` kick off async task (via `TaskRunner`) untuk load data
- Task selesai → post event ke `EventBus` (atau `AsyncEventPoster`)
- Screen subscribe ke event → set `dirty_ = true` → `requestRedraw()`

**Option B**: Load di background saat screen masuk, invalidate saat done
- `StorageService` cache hasil scan, refresh di background
- Event `StorageDataReady` → semua storage screens redraw

Untuk Plan 89, implementasi **minimal**: `StorageSettingsScreen` dan
`AppDetailScreen` load data async dan refresh saat selesai. Bukan full reactive system —
itu scope terpisah (Plan 90 kalau diperlukan).

### 3.3 Checklist Fase 3

- [x] Tambah `loadAsync()` di `StorageSettingsScreen::onResume()` — kick `TaskRunner` job
- [x] `TaskRunner` job: scan `/system/data` + `/sd/data` → store result
- [x] Saat done: `asyncPoster_.post({events::StorageStatsReady, ...})`
- [x] `StorageSettingsScreen` subscribe `StorageStatsReady` → `dirty_ = true` + redraw
- [x] Sama untuk `AppDetailScreen` — `allApps()` bisa lambat di device
- [x] Test: buka Storage screen → angka muncul tanpa perlu tekan tombol

---

## 4. Fase 4 — SD card info di Storage settings

### 4.1 Masalah

Storage settings hanya menampilkan ukuran Internal Flash dan SD Card (kalau mounted).
Tidak ada info: merk/model SD, total kapasitas, opsi eject.

### 4.2 Solusi

Extend `StorageService` + `StorageSettingsScreen`:

```cpp
struct SdCardInfo {
    bool    mounted      = false;
    std::string vendor;    // CID register bits
    std::string name;      // product name from CID
    uint64_t totalBytes  = 0;
    uint64_t freeBytes   = 0;
};
SdCardInfo sdCardInfo() const;
```

UI additions di `StorageSettingsScreen`:
- Row "SD Card" → kalau mounted: `"[name] — X GB free"`
- Kalau tidak mounted: `"Not mounted"`
- Tambah action row "Eject SD Card" (call `StorageService::ejectSd()`) kalau mounted

`ejectSd()` → unmount FAT filesystem, post event `SdEjected` → WifiService + semua
service yang punya file handle harus flush dan close.

### 4.3 Checklist Fase 4

- [x] Extend `StorageService` dengan `sdCardInfo()` → baca CID dari SDSPI driver
- [x] Extend `StorageService` dengan `ejectSd()` → `vfs_->unmount("/sd")`
- [x] Update `StorageSettingsScreen` — tampilkan info SD + eject button
- [x] Test eject: SD unmount sukses, re-insert dideteksi (kalau ada hotplug event)

---

## 5. Prioritas & urutan

| Fase | Effort | Impact | Urutan |
|---|---|---|---|
| Fase 1 (permission wire-up) | Kecil (2-3 jam) | Tinggi — permission dialog muncul, status benar | **Pertama** |
| Fase 2 (resource lease) | Sedang (1 hari) | Tinggi — WiFi chip tidak konflik | **Kedua** |
| Fase 3 (stateful screen) | Sedang (4-6 jam) | Sedang — UX storage screen lebih baik | **Ketiga** |
| Fase 4 (SD card info) | Kecil-sedang | Rendah-sedang — nice to have | **Keempat** |

Fase 1 dan 2 **blocking** untuk WiFi Marauder production-ready. Fase 3 dan 4 adalah
quality improvements.

---

## 6. Definition of Done

- [x] WiFi Marauder: dialog permission muncul pertama kali app coba scan/monitor
- [x] WiFi Marauder: Settings → Apps → WiFi Marauder menampilkan permission toggles
- [x] WiFi Marauder: Monitor Mode → sistem WiFi pause; kembali ke main → resume
- [x] Storage settings: angka muncul tanpa perlu tekan tombol
- [x] Storage settings: SD card info + eject option tersedia
- [x] Test semua di skyrizz-e32 + WASM simulator
- [x] Update `docs/STATE.md`
- [x] Commit konvensional

---

## 7. Files yang akan dimodifikasi

| File | Perubahan |
|---|---|
| `firmware/core/src/wasm/wasm_wifi.cpp` | Tambah `checkPerm()`, guard tiap binding |
| `firmware/core/include/nema/wasm/wasm_engine.h` | Pastikan `WasmHostCtx` punya `bundleId` |
| `firmware/core/include/nema/system/leasable.h` | Baru — `ILeasable` interface |
| `firmware/core/include/nema/system/resource_broker.h` | Baru — `ResourceBroker` |
| `firmware/core/src/system/resource_broker.cpp` | Baru — impl |
| `firmware/platforms/esp32/src/services/wifi_service.cpp` | Implement `ILeasable` |
| `firmware/core/src/screens/storage_settings_screen.cpp` | Async load + event subscribe |
| `firmware/core/src/screens/app_detail_screen.cpp` | Async load + event subscribe |
| `firmware/core/include/nema/services/storage_service.h` | `sdCardInfo()`, `ejectSd()` |
| `firmware/core/src/services/storage_service.cpp` | Impl SD info + eject |
| `firmware/core/src/screens/storage_settings_screen.cpp` | SD card section + eject button |
