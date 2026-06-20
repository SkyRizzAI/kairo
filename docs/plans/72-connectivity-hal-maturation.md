# 72 — Connectivity HAL Maturation (WiFi · BLE · Net)

> **Matangkan lapisan API (HAL) konektivitas** sebelum membangun UI & remote di atasnya.
> Tujuannya: kontrak HAL WiFi/BLE/Net yang **lengkap, jujur (punya state machine), dan
> enak diimplementasi board lain** — bukan "asal jalan". Ini fondasi seri Connectivity &
> Network MVP (72 → 73 → 74 → 75).

- Status: 🔴 Not started
- Depends on: 20 (WiFi), 34 (BLE), 42 (Capability-Resource model), 48 (System API IDL)
- Blocks: 73 (Connectivity Settings UI), 74 (Remote auth), 75 (TCP transport), 62 (NTP)

---

## 0. Konteks & masalah

Driver WiFi & BLE **sudah ada dan ter-register** (lewat `Esp32Platform::registerDrivers`
→ `wifi_.onRegister` menambah `caps::NetWifi`, `ble_.onRegister` menambah `caps::BtBle`),
tapi kontrak HAL-nya **belum cukup matang untuk menyetir UI yang benar**:

1. **WiFi tak punya state machine eksplisit.** `IWifiDriver::isConnected()` cuma boolean.
   UI tak bisa membedakan `Connecting` vs `Connected` vs `Wrong password` vs `No AP` vs
   `Disconnected` — padahal plan 63 TC-03/TC-04 menuntut itu. Status connection adalah
   informasi yang **hilang** di kontrak sekarang.
2. **Tak ada model "online" terpusat.** HTTP digate ke status WiFi secara ad-hoc; tidak ada
   satu sumber kebenaran "device punya konektivitas IP".
3. **Capability statis, tak melaporkan liveness.** `caps::NetWifi`/`caps::BtBle` cuma
   `add()`-ed; tak pernah `setState()` (Available/Fault/Disabled) walau model itu **sudah
   tersedia** (plan 42, `CapabilityRegistry::setState/available`). UI tak bisa menampilkan
   "WiFi: hardware fault".
4. **Saved networks single-slot.** Auto-reconnect hanya menyimpan 1 SSID di NVS. Tak ada
   manajemen profil banyak jaringan.
5. **IDL belum paritas.** `api/net.pidl` menandai `interface wifi` sebagai `@future`;
   `api/bt.pidl` baru `enable/disable/is-enabled`. SSOT belum mencerminkan HAL penuh.
6. **Kontrak driver-author belum terdokumentasi.** Tak ada checklist "apa yang WAJIB vs
   OPSIONAL diimplementasi board baru", sehingga porting ke board lain menebak-nebak.

> Catatan: BLE adapter (`IBleAdapter`) sebenarnya sudah cukup kaya (advertise, GATT,
> pairing numeric, bonding, central scan opsional). Pekerjaan BLE di plan ini lebih ke
> **state/liveness + status query untuk UI**, bukan menulis ulang.

---

## 1. Goals (Definition of "matang")

- [ ] WiFi punya **status query first-class** (enum state + alasan gagal) — UI & remote bisa
      menampilkan keadaan sebenarnya tanpa menebak dari boolean.
- [ ] **Capability liveness** dipakai: WiFi/BLE `setState()` Available/Fault/Disabled, dan
      memublikasi `ResourceChanged` — konsisten dengan model plan 42.
- [ ] **Satu konsep konektivitas** (`isOnline()` / `NetStatus`) yang menjadi gate tunggal
      untuk HTTP/NTP/remote-TCP.
- [ ] **Saved networks**: simpan ≥N profil di NVS, auto-pick saat reconnect.
- [ ] **IDL paritas penuh**: `net.pidl` + `bt.pidl` mencerminkan HAL final (hapus `@future`
      pada method yang sudah ada).
- [ ] **Kontrak driver-author**: dokumen "WAJIB vs OPSIONAL" + `NullWifiDriver`/sim sebagai
      referensi implementasi, sehingga board baru = isi method, bukan reverse-engineer.
- [ ] Semua method punya **kontrak threading** yang ditulis (blocking → worker, query → UI).

**Non-goal (di luar plan ini):** UI screens (→ 73), auth/remote (→ 74), TCP transport (→ 75),
WiFi SoftAP / mesh, BLE Classic.

---

## 2. Desain — WiFi (`hal/wifi.h`)

### 2.1 State machine eksplisit

Tambah enum status + alasan, dan query-nya. Ini perubahan inti:

```cpp
enum class WifiState : uint8_t {
    Disabled,      // radio off / driver tak aktif
    Idle,          // on, tak terhubung
    Scanning,      // scan in-flight
    Connecting,    // assoc/auth in-flight
    Connected,     // assoc + IP didapat
    Failed,        // percobaan terakhir gagal (lihat lastError)
};

enum class WifiError : uint8_t {
    None, AuthFailed,    // password salah
    ApNotFound, Timeout, DhcpFailed, Unknown,
};

struct IWifiDriver : IDriver {
    // ── status (UI thread, non-blocking) ──
    virtual WifiState state()      const = 0;
    virtual WifiError lastError()  const = 0;
    virtual int8_t    rssi()       const = 0;   // RSSI AP yang sedang terhubung, 0 jika tak

    // ... method existing tetap (connect/disconnect/scan/scanResults/ip/ipConfig) ...
};
```

- Driver **WAJIB** memublikasi event saat transisi state (lewat `AsyncEventPoster` dari
  worker → main): `NetworkConnected`, `NetworkDisconnected`, `WifiScanComplete`, dan event
  baru **`WifiStateChanged {"state":"connecting","err":"none"}`** (tambah di `event/event.h`).
- `connect()` jadi **non-blocking initiate** + state machine, ATAU tetap blocking tapi
  meng-update state sebelum/sesudah — **pilih: blocking di worker** (konsisten dgn kontrak
  Nema sekarang), tapi `state()` di-set `Connecting` sebelum block dan `Connected/Failed`
  sesudah, sehingga UI thread bisa polling/menerima event.

### 2.2 Saved networks (multi-profil)

```cpp
struct WifiProfile { char ssid[33]; bool secured; /* password di NVS terpisah */ };

struct IWifiDriver : IDriver {
    virtual void saveNetwork(const char* ssid, const char* password) = 0;
    virtual void forgetNetwork(const char* ssid) = 0;
    virtual size_t savedCount() const = 0;
    virtual bool   savedAt(size_t i, WifiProfile& out) const = 0;
    virtual void   autoConnect() = 0;   // pilih profil tersimpan dgn RSSI terbaik
};
```

- ESP32: simpan list di NVS namespace `wifi` (key `n0..nN` ssid, `p0..pN` password,
  ≤15 char/ key sesuai limit NVS). Password **tidak** plaintext di log; pertimbangkan
  enkripsi NVS (di luar scope, catat sebagai known-issue).
- Sim (`SimWifiDriver`): simpan di RAM; tetap implementasi penuh agar UI bisa diuji.

### 2.3 Country code & antenna (hardening dari plan 63)

- Driver ESP32 set country code (`esp_wifi_set_country` → `"ID"`/`"US"`) di `onRegister`.
- Dokumentasikan di `board_config.h` skyrizz: tipe antena (PCB/u.FL) + catatan koeksistensi.

### 2.4 Capability liveness

- `onRegister`: setelah init sukses → `caps.setState(NetWifi, Available)`; jika
  `esp_wifi_init` gagal → `setState(NetWifi, Fault)`.
- (cek nilai enum `ResourceState` aktual di `system/resource_state.h`.)

---

## 3. Desain — Net (konektivitas terpusat)

Buat **satu sumber kebenaran "online"**, dipakai HTTP/NTP/remote-TCP:

- Tambah `bool Runtime::isOnline()` ATAU service ringan `NetStatusService` yang men-subscribe
  `NetworkConnected/Disconnected` dan mengekspos `isOnline()`. Pilih `NetStatusService`
  (lebih bersih; remote-TCP listener nanti bisa subscribe untuk bind/unbind socket).
- `IHttpClient` users & NTP (`plan 62`) memanggil gate ini, bukan `wifi.isConnected()`
  langsung — sehingga nanti konektivitas via Ethernet/PPP/cellular bisa ditambah tanpa
  mengubah pemanggil.

---

## 4. Desain — BLE (`hal/bluetooth.h`)

Adapter sudah kaya; tambahkan **status untuk UI + liveness**:

- `IBluetoothController`: pastikan `isEnabled()`, `mode()`, `address()`, `deviceName()`
  cukup untuk header settings. Tambah liveness: `enable()` sukses → `setState(BtBle, Available)`;
  gagal → `Fault`; `disable()` → `Disabled`.
- `IBleAdapter`: tambah `int8_t rssi()` untuk koneksi aktif (opsional, default 0) — dipakai
  list bonded/connected di UI.
- Pairing/bonding sudah ada (`onPairRequest`/`confirmPairing`/`bondedAt`/`forget`). Pastikan
  event `BtPairRequest/BtPaired/BtConnected/BtDisconnected` dipublikasi konsisten dari stack.
- Central scan (`startScan/connectTo`, plan 67) tetap **opsional** (default no-op) — board
  tanpa central tetap valid.

---

## 5. IDL paritas (SSOT — `api/`)

- `api/net.pidl`: lepas `@future` dari `interface wifi`; tambah `state() -> wifi-state`,
  `rssi() -> s8`, `saved-networks`, selaras §2. Tambah record `wifi-state` enum.
- `api/bt.pidl`: lengkapi `interface ble` → `advertise`, `is-advertising`, `address`,
  `device-name`, `bonded-list`, selaras HAL. Tandai parity vs `hal/bluetooth.h` di tabel.
- Update `api/README.md` tabel parity (kolom "v0 parity") untuk WiFi/BLE.
- (Generator binding JS/WASM = plan 49; plan ini hanya **mendefinisikan** SSOT-nya.)

---

## 6. Kontrak driver-author (output kunci: "enak untuk board lain")

Buat **`firmware/boards/README.md` → section "Implementing connectivity"** (atau
`docs/architecture/connectivity.md`) yang mendaftarkan:

| Method | WiFi | BLE | WAJIB? |
|---|---|---|---|
| `connect/disconnect/state/isConnected/scan/scanResults/ip` | ✅ | — | **WAJIB** |
| `saveNetwork/forget/autoConnect` | ✅ | — | WAJIB (boleh RAM-only di board tanpa NVS) |
| `rssi/lastError/ipConfig` | ✅ | — | WAJIB |
| `enable/disable/isEnabled/advertise/notify/onWrite/pairing/bonding` | — | ✅ | **WAJIB** (BLE periph) |
| `startScan/connectTo` (central) | — | ✅ | OPSIONAL (default no-op) |

- Sertakan **`NullWifiDriver`** (RAM-only, selalu "Connected" ke 1 fake AP) di
  `platforms/wasm` / test sebagai **referensi implementasi minimal** + dipakai host tests.
- Sertakan **conformance test** di `firmware/tests/` (`wifi_contract_test`): menjalankan state
  machine terhadap driver mock dan memverifikasi transisi + event dipublikasi.

---

## 7. Tasks

- [ ] `hal/wifi.h`: tambah `WifiState`/`WifiError` + `state()/lastError()/rssi()` + saved-network API.
- [ ] `event/event.h`: tambah `WifiStateChanged`.
- [ ] `esp32_wifi_driver`: implement state machine, event publish (via AsyncEventPoster),
      saved networks (NVS), country code, `setState` liveness.
- [ ] `SimWifiDriver` (wasm): implement state machine + saved networks (RAM) penuh.
- [ ] `NullWifiDriver` + `wifi_contract_test` (host).
- [ ] `NetStatusService`: `isOnline()` terpusat; HTTP/NTP gate di-route ke sini.
- [ ] `hal/bluetooth.h`: tambah `rssi()` opsional + liveness `setState` di controller.
- [ ] `esp32_ble`: publish event + `setState` konsisten.
- [ ] `api/net.pidl` + `api/bt.pidl` + `api/README.md`: paritas penuh.
- [ ] Dokumen kontrak driver-author (board connectivity).
- [ ] Build hijau di 3 target (wasm/dev-board/skyrizz) + `bun run test` lulus.

## 8. Acceptance criteria

- [ ] `wifi.state()` melaporkan `Connecting → Connected` dan `Connecting → Failed(AuthFailed)`
      dengan benar di sim **dan** di hardware (diverifikasi di plan 73).
- [ ] `caps.available("net.wifi")`/`available("bt.ble")` mencerminkan keadaan radio nyata.
- [ ] `isOnline()` satu-satunya gate untuk HTTP/NTP; tak ada pemanggil yang menyentuh
      `wifi.isConnected()` langsung (kecuali WiFi UI).
- [ ] ≥2 saved network bisa disimpan, di-forget, dan `autoConnect()` memilih RSSI terbaik.
- [ ] `wifi_contract_test` hijau; `NullWifiDriver` lulus kontrak.
- [ ] `net.pidl`/`bt.pidl` tak ada `@future` untuk method yang sudah diimplementasi.
- [ ] Tak ada `#include` platform-spesifik bocor ke `core/**`.
