# 34 — Connectivity Foundation: Bluetooth/BLE + USB

> **LAYER 1 dari 3** (Foundation → Remote Layer (35) → Forge (36)). Plan ini
> membangun **fondasi konektivitas UMUM & multi-fungsi** — bukan cuma untuk
> remote. BLE & USB adalah medium yang dipakai BANYAK fitur; remote (Plan 35)
> hanya **salah satu** konsumen. Karena itu HAL-nya didesain **extensible**
> (multi-service / multi-class), bukan satu pipa tunggal.
>
> 1. **Bluetooth/BLE** — `IBluetoothController` membawahi `IBleAdapter` +
>    `IClassicAdapter`, dipilih lewat capability bukan tipe board. SkyRizz E32
>    (ESP32-S3) = **BLE-only**. Sekarang: peripheral/GATT-server (advertise,
>    pairing LE Secure, bond) + **registrasi banyak GATT service** (remote PLP =
>    satu; custom-app bisa daftar service sendiri untuk tukar data).
>    Dipersiapkan: **GATT client** (connect ke device lain) & **A2DP** (sambung
>    speaker / teruskan audio) sebagai profil future.
> 2. **USB** — native USB ESP32-S3 via TinyUSB sebagai **composite device**:
>    **CDC-ACM** (data/serial) sekarang; dipersiapkan **MSC** (mount microSD ke
>    komputer / transfer file) & HID future.
>
> Plan ini IMPLEMENTASI minimal yang dibutuhkan remote (BLE GATT-server+bond,
> USB-CDC), TAPI **arsitektur disiapkan** untuk konsumen lain (A2DP, MSC,
> custom-app data exchange, BLE central). Framing/protokol (PLP) ada di Plan 35;
> Layer 1 menyediakan **byte pipe + connection mgmt + profil/class extensible**.

- Status: ✅ BLE selesai (host + ESP32-S3). Host-side: HAL (`bluetooth.h`,`usb_cdc.h`) + Bt* events + SimBleAdapter/Controller + registrasi sim + capability `bluetooth`/`bluetooth.ble` + command `ble_phone_*` + BluetoothApp (ComponentApp) + Settings gated; host build hijau, smoke test pairing (BtPairRequest+passkey) ✓. **ESP32-S3: `Esp32Ble` (NimBLE) — advertise + GATT server + LE Secure Connections (numeric-comparison) + bond di NVS; di-`onRegister` oleh `Esp32Platform` (capability `bluetooth.ble`). `idf.py build` hijau → `palanu-skyrizz-e32.bin` (70% flash free).** Verifikasi pairing fisik (HP) menunggu device terhubung. USB-CDC: lihat §6 — transport PLP siap (`UsbCdcLinkTransport`). **(+Plan 37) USB-CDC remote AKTIF**: `Esp32UsbCdc` (HWCDC `Serial` = USB-Serial-JTAG, reader-task) + `MuxTransport` gabung USB+BLE → satu RemoteService/screen-tap. Console berbagi pipa yang sama → PLP magic-byte resync menanganinya; untuk pipa bersih sempurna, set console off-USB (1 baris sdkconfig). Build dua device hijau; verifikasi colok fisik menunggu device.
- Milestone: M7 (Connectivity)
- Depends on: **19.5 (Nema: Thread/TaskRunner)**, **19.6 (App-model: IApp/AppHost/AppContext)**, 16 (ESP32 Platform), 24 (Config Store), 30 (Component runtime + screen migration)
- Blocks: **35 (Nema Link Protocol & Remote Layer)**, OTA

> Kontrak kejujuran (sama seperti 19.5/20): tidak ada klaim ajaib; tiap fase
> build & jalan di host+ESP32; race ditangani by-design; verifikasi pairing
> nyata di hardware (HP + nRF Connect).

---

## 0. Latar belakang & keputusan kunci

Palanu belum punya sistem Bluetooth sama sekali. WiFi (Plan 20) sudah membuktikan
pola yang benar untuk konektivitas: **driver di platform**, **app di thread
sendiri** (`WifiApp` via AppHost), kerja blocking di **TaskRunner worker**, event
balik via **AsyncEventPoster**. Bluetooth mengikuti pola yang sama persis.

Pertanyaan desain dari diskusi: *"BT Classic dan BLE, bisa ga jadi satu sistem?"*
Jawaban: **bisa**, dan harus — lewat satu controller + dua sub-adapter.

### Fakta hardware yang mengunci desain

| Chip | Classic BT | BLE | Catatan |
|---|---|---|---|
| ESP32 (asli) | ✅ | ✅ | dual-mode |
| **ESP32-S3** (SkyRizz E32) | ❌ | ✅ | **BLE only** — tidak ada Classic |
| ESP32-C3/C6/H2 | ❌ | ✅ | BLE only |

Artinya: abstraksi harus **mengakomodasi** Classic (untuk board/chip masa depan
yang punya), tetapi implementasi konkret untuk SkyRizz **hanya BLE**. Ini contoh
sempurna desain capability-driven:
- `capabilities().has("bluetooth")` → true (radio ada)
- `capabilities().has("bluetooth.ble")` → true
- `capabilities().has("bluetooth.classic")` → **false** di ESP32-S3

App/UI cek capability, bukan nama chip. `IClassicAdapter` didefinisikan sebagai
interface tapi tidak diimplementasi di Esp32-S3 (future).

### Kenapa peran **peripheral / GATT server**?

Device akan **di-remote** oleh HP/desktop. Maka device = **peripheral** yang
*advertise* dan menunggu central (HP) connect. Ini peran yang tepat: HP scan →
connect → bonding. (Central role / scan device lain = future, tidak di plan ini.)

---

## 0b. Konsumen & fungsi (kenapa fondasi, bukan pipa tunggal)

Layer 1 harus melayani banyak fitur. Desain HAL mengakomodasi semua ini sejak
awal walau implementasinya bertahap:

| Medium | Fungsi | Konsumen | Status di plan ini |
|---|---|---|---|
| BLE | GATT server (PLP) | Remote (Plan 35) | ✅ implement |
| BLE | GATT service custom | Custom app (tukar data) | ✅ API siap (`registerService` multi) |
| BLE | GATT client (central) | Connect sensor/device lain | 🔜 interface, impl future |
| BLE | A2DP source/sink | Speaker / teruskan lagu | 🔜 profil future (catatan: A2DP butuh BT Classic — TIDAK di S3; LE Audio bila chip dukung) |
| USB | CDC-ACM | Remote/data, console | ✅ implement |
| USB | MSC (mass storage) | Mount microSD ke komputer, transfer file | 🔜 composite siap, impl future |
| USB | HID | Keyboard/gamepad emul | 🔜 future |

Prinsip: **`IBleAdapter` mengizinkan banyak GATT service** (remote + custom-app
masing-masing daftar service-nya), dan **USB = composite device** (CDC + MSC + …
hidup bersamaan via TinyUSB). Jadi menambah fitur = menambah service/class, BUKAN
merombak Layer 1.

> ⚠️ Catatan audio jujur: **A2DP klasik tidak ada di ESP32-S3** (BLE-only, no BT
> Classic). "Sambung speaker / teruskan lagu" via Bluetooth di S3 = **LE Audio**
> (butuh stack & dukungan yang masih terbatas) atau lewat board/chip yang punya BT
> Classic. Jadi A2DP ditandai future + capability-gated; arsitektur controller
> sudah menyediakan `BtMode`/profil agar siap saat hardware/stack mendukung.

---

## 1. Goal

Dari device, tanpa flash ulang:
1. Nyalakan/matikan Bluetooth (toggle di Settings).
2. Lihat status: nama device, alamat (MAC), advertising/connected.
3. Masuk mode "Discoverable" → device advertise, HP bisa menemukannya.
4. Pairing aman: HP minta pair → device tampilkan **6-digit code** di LCD →
   user konfirmasi (numeric comparison, anti-MITM) → bonded.
5. Lihat daftar **Paired Devices** → "Forget" satu / semua.
6. Bonding **persist** (NVS) — reconnect otomatis setelah reboot.

**Simulator:** panel web meniru HP (connect, kirim passkey, confirm) supaya
seluruh alur BluetoothApp bisa dites tanpa hardware. **SkyRizz E32:** BLE nyata
via NimBLE, LE Secure Connections, bond di NVS.

---

## 2. Arsitektur (app-model, paralel WiFi)

```
BluetoothApp (thread, via AppHost — ComponentApp, status bar tampil)
  state machine: Overview → Discoverable → PairConfirm → Paired → Result
  │
  ├─ enable/disable: IBluetoothController.enable(BtMode::Ble) / disable()
  │     (init stack berat → submit ke TaskRunner worker; app gambar "Starting…")
  │
  ├─ advertise: IBleAdapter.startAdvertising()  → status "Discoverable as <name>"
  │
  ├─ pairing: stack raih onPairRequest{passkey,addr} (via AsyncEventPoster → app)
  │     → app render modal "Pair? 4 8 3 9 2 1  [OK]/[Back]"
  │     → confirmPairing(accept) → stack lanjut LE Secure → bonded
  │
  └─ paired list: bondedCount()/bondedAt() (baca thread-safe), forget()
```

Semua di-render via component tree (`ComponentApp::build`), input via
`onAction`/Pressable. Kerja blocking (init radio) via `ctx.runtime().tasks()`.
Tidak ada operasi BLE yang membekukan UI.

```
┌────────────── core (hardware-agnostic) ──────────────┐
│ hal/bluetooth.h                                       │
│   IBluetoothController  (radio: enable/disable/mode)  │
│   IBleAdapter           (advertise/connect/pair/bond) │
│   IClassicAdapter       (interface only — future)     │
│ apps/bluetooth_app.{h,cpp}  (UI state machine)        │
└───────────────────────────────────────────────────────┘
              ▲ register + capabilities          ▲ events
┌─────────────┴──────────┐         ┌─────────────┴──────────┐
│ platforms/simulator    │         │ platforms/esp32        │
│  SimBleAdapter         │         │  Esp32BleAdapter       │
│  (web "phone" model)   │         │  (NimBLE + NVS bond)   │
└────────────────────────┘         └────────────────────────┘
```

---

## 3. HAL (`core/include/palanu/hal/bluetooth.h`) — BARU

```cpp
#pragma once
#include "nema/hal/driver.h"
#include <cstdint>
#include <cstddef>

namespace nema {

enum class BtMode : uint8_t { Off, Ble, Classic, Dual };

// Peer identity. POD — aman dicopy antar-thread.
struct BtPeer {
    char    name[32] = {};
    uint8_t addr[6]  = {};
    bool    bonded   = false;
};

// Permintaan pairing (numeric comparison / passkey display).
struct BlePairRequest {
    uint32_t passkey = 0;     // 6-digit code untuk ditampilkan di layar
    uint8_t  addr[6] = {};
};

// GATT server definition (peran peripheral). Bitmask properti.
namespace BleProp { enum : uint8_t { Read=1, Write=2, Notify=4 }; }
struct BleCharacteristic { const char* uuid; uint8_t props; };
struct BleService        { const char* uuid; const BleCharacteristic* chars; uint8_t charCount; };

// ── Radio controller (dipakai bersama BLE + Classic) ──
struct IBluetoothController : IDriver {
    DriverKind kind() const override { return DriverKind::Bluetooth; }
    // enable() init stack — BOLEH blocking, dipanggil dari TaskRunner worker.
    virtual bool        enable(BtMode mode) = 0;
    virtual void        disable() = 0;
    virtual bool        isEnabled() const = 0;
    virtual BtMode      mode() const = 0;
    virtual const char* address() const = 0;          // MAC lokal "AA:BB:..."
    virtual void        setDeviceName(const char* name) = 0;
    virtual const char* deviceName() const = 0;
};

// ── BLE peripheral adapter (advertise + GATT server + pairing + bonding) ──
struct IBleAdapter : IDriver {
    DriverKind kind() const override { return DriverKind::Bluetooth; }

    // Definisi GATT server — dipanggil sekali sebelum advertising. Plan 34 cukup
    // daftar Device Info Service minimal; Plan 35 mendaftarkan service PLP.
    virtual void registerService(const BleService& svc) = 0;

    // Advertising.
    virtual bool startAdvertising() = 0;
    virtual void stopAdvertising()  = 0;
    virtual bool isAdvertising() const = 0;

    // Connection state.
    virtual bool isConnected() const = 0;
    virtual bool peer(BtPeer& out) const = 0;
    virtual void disconnect() = 0;

    // I/O primitif (server side) — dipakai Plan 35 untuk PLP.
    virtual bool notify(const char* charUuid, const uint8_t* data, size_t len) = 0;
    using WriteFn = void(*)(void* user, const char* charUuid, const uint8_t* data, size_t len);
    virtual void onWrite(WriteFn fn, void* user) = 0;

    // Pairing: stack raih request → app tampilkan passkey → confirmPairing().
    using PairFn = void(*)(void* user, const BlePairRequest& req);
    virtual void onPairRequest(PairFn fn, void* user) = 0;
    virtual void confirmPairing(bool accept) = 0;

    // Bonded peers (persist di NVS oleh stack).
    virtual size_t bondedCount() const = 0;
    virtual bool   bondedAt(size_t i, BtPeer& out) const = 0;
    virtual void   forget(const uint8_t addr[6]) = 0;
    virtual void   forgetAll() = 0;
};

// ── Classic adapter — interface only (future; tak ada impl di ESP32-S3) ──
struct IClassicAdapter : IDriver {
    DriverKind kind() const override { return DriverKind::Bluetooth; }
    virtual bool startDiscoverable() = 0;   // SPP/A2DP future
    virtual void stopDiscoverable()  = 0;
};

} // namespace nema
```

**Catatan threading (jujur, sama kontrak WiFi):** `enable()` boleh blocking →
worker thread. `notify()` dipanggil dari main/app thread. Callback `onWrite`/
`onPairRequest` dipanggil dari **host stack task** (NimBLE) → WAJIB di-rebounce
lewat **AsyncEventPoster** ke main task sebelum menyentuh UI/Canvas.
`bondedCount()/peer()` baca sederhana (snapshot), aman dari app thread.

---

## 4. Events (`core/include/palanu/event/event.h`)

```cpp
inline constexpr const char* BtEnabled       = "BtEnabled";       // {}
inline constexpr const char* BtDisabled      = "BtDisabled";      // {}
inline constexpr const char* BtPairRequest   = "BtPairRequest";   // {"passkey":"483921"}
inline constexpr const char* BtPaired        = "BtPaired";        // {"name":"iPhone"}
inline constexpr const char* BtConnected     = "BtConnected";     // {"name":"iPhone"}
inline constexpr const char* BtDisconnected  = "BtDisconnected";  // {}
```

Diposting dari host-stack callback via `AsyncEventPoster` (aman). BluetoothApp
membaca alur utama via callback langsung; event tetap ada untuk status bar / log /
konsumen lain (mis. ikon BT di status bar).

---

## 5. Driver implementations

### 5.1 SimBleAdapter (`platforms/simulator`)
- `SimBluetoothController`: `enable()` set flag + dummy MAC `"00:11:22:33:44:55"`,
  emit `BtEnabled`. `setDeviceName` simpan.
- `SimBleAdapter`: `startAdvertising()` set flag, broadcast status. Pairing &
  connection **disetir panel web** (meniru HP):
  - cmd `ble_phone_connect` → adapter set "incoming pair", generate passkey
    pseudo-acak (mis. dari counter), raih `onPairRequest` → BluetoothApp tampilkan
    modal. (Sim tak punya entropi nyata; passkey deterministik per koneksi OK.)
  - cmd `ble_phone_confirm{accept}` → kalau device juga sudah confirm → bonded,
    tambah ke `bonded_` (in-memory; persist via MemConfigStore biar survive
    restart sim), emit `BtPaired`+`BtConnected`.
  - cmd `ble_phone_disconnect` → emit `BtDisconnected`.
- `notify()`/`onWrite` di-bridge ke panel web (dipakai Plan 35). Plan 34 cukup
  log saja.

### 5.2 Esp32BleAdapter (`platforms/esp32`) — NimBLE
- Pakai **NimBLE** (ringan, hemat flash/heap dibanding Bluedroid). Enable di
  sdkconfig target: `CONFIG_BT_ENABLED=y`, `CONFIG_BT_NIMBLE_ENABLED=y`,
  Bluedroid off.
- `enable(BtMode::Ble)`: `nimble_port_init()` + start host task. (BLE-only:
  `BtMode::Classic/Dual` → return false di S3.)
- GATT: daftarkan service dari `registerService()`. Plan 34 = Device Info Service
  (0x180A): Manufacturer, Model, FW Version (`rt.info()`).
- Advertising: nama device + service UUID, connectable.
- **Security (inti plan ini):** LE Secure Connections + **numeric comparison**
  (MITM-protected). Set:
  - `ble_hs_cfg.sm_sc = 1` (Secure Connections)
  - `ble_hs_cfg.sm_mitm = 1`, `sm_bonding = 1`
  - IO capability `BLE_HS_IO_DISPLAY_YESNO` → memicu numeric comparison
  - Callback `BLE_GAP_EVENT_PASSKEY_ACTION` (action `NUMCMP`) → raih
    `BlePairRequest{passkey}` via AsyncEventPoster → app modal → `confirmPairing`
    → `ble_sm_inject_io(... numcmp accept ...)`.
- **Bonding NVS:** NimBLE simpan bond store di NVS otomatis
  (`ble_store_config_init()`). `bondedCount/bondedAt` baca via
  `ble_store_util_count`/iterate. `forget` → `ble_store_util_delete_peer`.
- Host-stack callback → semua diteruskan ke main task lewat **AsyncEventPoster**.
- Owned oleh **Esp32Platform** (seperti `Esp32WifiDriver`): radio = milik SoC,
  bukan board. Platform `registerDrivers()` daftarkan controller+adapter dan
  `capabilities().add("bluetooth"/"bluetooth.ble")`.

---

## 6. BluetoothApp (`core/apps/bluetooth_app.{h,cpp}`) — ComponentApp

```cpp
class BluetoothApp : public ComponentApp {
    const char* id()   const override { return "com.palanu.bluetooth"; }
    const char* name() const override { return "Bluetooth"; }
protected:
    ui::UiNode* build(ui::NodeArena& a, AppContext& ctx) override;
    ui::UiNode* buildModal(ui::NodeArena& a, AppContext& ctx) override; // PairConfirm
    bool onTick(AppContext& ctx) override;     // poll state/flags
    uint32_t tickIntervalMs() const override { return 200; }
};
```

State: `Overview, Starting, Discoverable, PairConfirm, Paired, Result`.
- **Overview**: toggle On/Off, nama device, MAC, status (Off/Idle/Advertising/
  Connected), menu: [Discoverable] [Paired Devices] [Forget All].
- **Starting**: submit `enable()` ke worker; gambar "Starting Bluetooth…".
- **Discoverable**: `startAdvertising()`; "Discoverable as <name> — open the app on
  your phone". Hint label via `rt.input().hintFor(...)` (jangan hardcode).
- **PairConfirm** (modal): tampilkan 6-digit passkey besar + Pressable
  [Pair]/[Cancel] → `confirmPairing(accept)`.
- **Paired**: list `bondedAt()` (nama + bonded ✓), Pressable per item → Forget.
- **Result**: sukses/gagal → balik Overview.

### Settings integration
`SettingsScreen` (sudah ComponentScreen, Plan 30) tambah item **"Bluetooth"**
muncul **hanya bila** `capabilities().has("bluetooth")` → `onSelect`: launch
`BluetoothApp` via AppHost (pola identik item "WiFi"). Status bar bisa tambah ikon
BT (opsional, bila `BtConnected`).

---

## 7. Keamanan (rangkuman — kenapa aman)

| Lapis | Mekanisme | Lindungi dari |
|---|---|---|
| Transport | **LE Secure Connections** (ECDH P-256) + enkripsi | eavesdropping, passive sniff |
| Pairing | **Numeric comparison** (user confirm 6-digit di kedua sisi) | MITM / device palsu |
| Persistence | **Bonding** di NVS, IO cap DISPLAY_YESNO | re-pair tiap connect |
| Otorisasi | **Forget device** (revoke bond) di Settings | HP hilang/dicuri |

> Lapis kedua (token handshake di level aplikasi) dibangun di **Plan 35 (PLP
> handshake)** — bisa di-revoke tanpa unpair BLE, mendukung multi-device & expiry.
> Plan 34 cukup mengamankan **transport + identitas**.

---

## 7b. USB foundation (composite: CDC + MSC + …)

Medium konektivitas kedua: **native USB ESP32-S3 sebagai composite device**
(TinyUSB) — beberapa class hidup bersamaan:
- **CDC-ACM** (data/serial) → dipakai remote (Plan 35) & bisa console.
- **MSC** (mass storage) → expose **microSD ke komputer** (mount, transfer file)
  — future, tapi composite disiapkan dari sekarang.
- **HID** → future.

Bagian di bawah fokus ke **CDC** (yang dibutuhkan remote). MSC/HID = penambahan
class pada composite yang sama, bukan rombak.

### HAL (`core/include/palanu/hal/usb_cdc.h`) — minimal

```cpp
struct IUsbCdc : IDriver {
    DriverKind kind() const override { return DriverKind::Other; }
    virtual bool   isOpen() const = 0;            // host membuka port?
    virtual size_t write(const uint8_t* d, size_t n) = 0;  // ke host
    using RecvFn = void(*)(void* user, const uint8_t* d, size_t n);
    virtual void   onData(RecvFn fn, void* user) = 0;       // dari host
};
```

### ESP32-S3 impl (`platforms/esp32`)
- Native USB via **TinyUSB CDC-ACM** (`esp_tinyusb`) — endpoint terpisah dari
  USB-Serial-JTAG yang dipakai console. Atau, bila console memakai
  USB-Serial-JTAG, CDC-ACM jadi data channel kedua.
- `write()` → `tinyusb_cdcacm_write_queue` + flush; RX callback → `onData`.
- Capability: `usb`, `usb.cdc`. Owned platform (radio/USB = milik SoC).

### Catatan
- USB-CDC = **byte pipe** murni; tak ada framing/pairing (USB sudah point-to-point
  & aman secara fisik). PLP handshake (Plan 35) tetap berlaku di atasnya.
- Tidak ada UI khusus: USB aktif saat kabel tertancap; remote layer mendeteksi
  `isOpen()`.

> **Status USB (jujur, per implementasi sekarang):**
> - ✅ Transport PLP generik **selesai**: `core/include/palanu/link/usb_cdc_link_transport.h`
>   (`UsbCdcLinkTransport` membungkus `IUsbCdc` → `ILinkTransport`). Board apa pun
>   yang menyediakan `IUsbCdc` langsung dapat PLP-over-USB tanpa kode tambahan.
> - ⏸️ **`Esp32UsbCdc` (device-side) DEFERRED di skyrizz-e32.** Alasan hardware
>   nyata: satu-satunya port USB board ini di-bond ke **USB-Serial-JTAG** untuk
>   console + auto-reset flashing yang andal (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`,
>   `ARDUINO_USB_MODE=1` → HWCDC). USB-Serial-JTAG dan TinyUSB-OTG berbagi PHY/pin
>   fisik yang sama (GPIO19/20) — hanya satu yang bisa aktif. Mengaktifkan TinyUSB
>   CDC akan mematikan console/flash path. Karena BLE + virtual-cable sudah menutup
>   kebutuhan remote, USB-device ditunda untuk board yang mendedikasikan USB-OTG ke
>   data (capability-gated, jadi tidak ada board yang rusak).

---

## 8. File Plan

| File | Aksi |
|---|---|
| `core/include/palanu/hal/bluetooth.h` | **baru** — controller + BLE/Classic adapters |
| `core/include/palanu/hal/usb_cdc.h` | **baru** — IUsbCdc byte pipe |
| `platforms/esp32/.../esp32_usb_cdc.{h,cpp}` | **baru** — TinyUSB CDC-ACM |
| `core/src/hal/bluetooth.cpp` | **baru** — helper emit (bila perlu) |
| `core/include/palanu/event/event.h` | + Bt* events |
| `core/include/palanu/apps/bluetooth_app.h` (+`.cpp`) | **baru** — state machine |
| `core/src/screens/settings_screen.cpp` | + item "Bluetooth" (gated capability) |
| `core/CMakeLists.txt` | + sources baru |
| `platforms/simulator/.../sim_ble_adapter.{h,cpp}` | **baru** — controller+adapter sim |
| `platforms/simulator/.../simulator_platform.cpp` | register + capability |
| `platforms/simulator/src/command_reader.cpp` | + `ble_phone_*` commands |
| `platforms/esp32/.../esp32_ble_adapter.{h,cpp}` | **baru** — NimBLE |
| `platforms/esp32/.../esp32_platform.cpp` | register + capability |
| `targets/skyrizz-e32/sdkconfig.defaults` | + NimBLE enable |
| `packages/simulator/components/ControlsPanel.tsx` | + Bluetooth "phone" panel |
| `packages/simulator/lib/useSimSocket.ts` | track BT state |

---

## 9. Fase Implementasi

1. **HAL + events** — `bluetooth.h`, Bt* events. Build dua target (adapter belum
   ada → tak ada yang implement, cukup header). 
2. **SimBleAdapter + panel web** — controller + adapter sim + command
   `ble_phone_connect/confirm/disconnect`. Tes alur di simulator headless/CLI.
3. **BluetoothApp + Settings** — ComponentApp state machine + modal PairConfirm +
   item Settings (gated). Tes penuh di simulator: enable → discoverable →
   (panel) phone connect → passkey muncul → confirm → bonded → forget.
4. **Esp32BleAdapter (NimBLE)** — enable, Device Info Service, advertising, LE
   Secure numeric comparison, bond NVS, connection state. sdkconfig. Flash +
   tes nyata dengan HP (nRF Connect / app Settings): pair → bonded → reboot →
   reconnect tanpa re-pair.
5. **Polish** — device name dari ConfigStore (Plan 24), forget all, ikon status
   bar, verifikasi `has("bluetooth.classic")==false` di S3.

Tiap fase: build host+ESP32, smoke test, baru lanjut.

---

## 10. Acceptance criteria

**Simulator**
- [ ] Toggle Bluetooth On → `BtEnabled`, status Advertising
- [ ] Panel "phone" connect → passkey muncul di BluetoothApp (modal)
- [ ] Confirm di device → bonded; muncul di Paired Devices
- [ ] Forget device → hilang dari list; restart sim → tetap hilang (persist)
- [ ] UI tak freeze saat "Starting…" (enable di worker)

**SkyRizz E32 (BLE nyata)**
- [ ] HP menemukan device saat Discoverable (nama benar)
- [ ] Pairing numeric comparison: 6-digit sama di device & HP, confirm → bonded
- [ ] Reboot device → HP reconnect otomatis tanpa re-pair (bond NVS)
- [ ] Forget device di Settings → HP harus pair ulang
- [ ] Salah/cancel passkey → pairing gagal, tidak bonded

**Cross-cutting**
- [ ] `has("bluetooth")` & `has("bluetooth.ble")` true; `has("bluetooth.classic")` **false** di S3
- [ ] Item "Bluetooth" di Settings hanya muncul bila capability ada
- [ ] `enable()` jalan di TaskRunner worker (UI frame terus keluar)
- [ ] Callback host-stack tidak menyentuh Canvas langsung (lewat AsyncEventPoster)

---

## 11. Non-Goals v1 (tapi ARSITEKTUR disiapkan — lihat §0b)
Implementasi sekarang fokus ke yang dibutuhkan remote (BLE GATT-server+bond,
USB-CDC). Berikut ditunda **tetapi HAL didesain agar tinggal nambah**, bukan rombak:
- **GATT service custom oleh app** — API `registerService` sudah multi; contoh app
  (tukar data) menyusul. (arsitektur ✅)
- **BLE central role** (connect ke sensor/device lain) — interface future.
- **A2DP / audio Bluetooth** (sambung speaker, teruskan lagu) — **butuh BT Classic
  (tidak ada di S3) atau LE Audio**; ditandai future + capability-gated. `BtMode`/
  profil di controller sudah menyiapkan slot-nya.
- **USB MSC** (mount microSD ke komputer / transfer file) & **HID** — composite USB
  disiapkan; class menyusul.
- **Channel data / PLP / remote / OTA** — Plan 35.
- Multiple koneksi aktif simultan (v1: 1 aktif; bond list boleh banyak).

---

## 12. Catatan threading (ringkas, penting)
- **Worker (TaskRunner):** `enable()` (init stack berat).
- **App thread (BluetoothApp::run):** state machine, draw, input, `confirmPairing`.
- **Host stack task (NimBLE):** GAP/GATT/SM callbacks → **AsyncEventPoster** → main task.
- **Main/GUI:** composit BluetoothApp + status bar.
- Tidak ada jalur di mana callback BLE menyentuh GUI/Canvas langsung. Sama
  disiplinnya dengan revisi WiFi Plan 20.
