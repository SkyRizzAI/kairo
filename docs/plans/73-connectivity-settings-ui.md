# 73 — Connectivity Settings UI (WiFi · Bluetooth) + Target Wiring + HW Verify

> Bangun kembali UI WiFi & Bluetooth yang **hilang saat rewrite UI/Aether** (commit
> `1230296` menghapus `wifi_app.cpp`/`bluetooth_app.cpp`), kali ini sebagai **screen di
> Settings** (bukan app terpisah), digate oleh capability, di-wire ke target skyrizz-e32 &
> dev-board, lalu **diverifikasi jalan di hardware**.

- Status: 🔴 Not started
- Depends on: **72 (Connectivity HAL Maturation)**, 60 (Settings ListView), 42 (capability-state)
- Blocks: 74 (Remote — butuh WiFi/BLE jalan), demo SkyRizz E32
- Supersedes: **63 (WiFi HW Verification)** — plan 63 merujuk `wifi_app.cpp` yang sudah
  dihapus; test case-nya dipindah & diperluas ke sini (§5).

---

## 0. Akar masalah (kenapa menu tak muncul di skyrizz)

Sudah dikonfirmasi dari kode, **bukan** soal driver:

1. `firmware/core/src/screens/settings_screen.cpp` (build, baris 44–56) **tidak punya entri
   WiFi/Bluetooth** — enum `Kind` hanya `About/Display/Controls/Touch/Sounds/Camera/Developer/
   Profile`. Section "Network" tak pernah ditambahkan.
2. `firmware/targets/skyrizz-e32/main/main.cpp` & `dev-board/main.cpp` **tidak meng-install**
   WifiApp/BluetoothApp (keduanya sudah dihapus dari core).

Jadi caps `net.wifi`/`bt.ble` ada, driver ada, **tapi tak ada satu pun UI yang menyetirnya.**

---

## 1. Goals

- [ ] **`WifiSettingsScreen`** — scan, list (signal bars + secured icon), connect via keyboard,
      disconnect, saved networks, status real (pakai `WifiState` dari plan 72).
- [ ] **`BluetoothSettingsScreen`** — enable toggle, advertise status, pairing modal (numeric),
      bonded-device list (forget), nama device.
- [ ] Keduanya **digate capability** di `SettingsScreen` + menampilkan state liveness
      (Available/Fault/Disabled) — bukan asal sembunyi.
- [ ] Di-wire ke **skyrizz-e32 & dev-board** (dan sim).
- [ ] **Terverifikasi di hardware skyrizz-e32**: WiFi + BLE **bersamaan** (koeksistensi radio).

**Non-goal:** remote/auth (→74), TCP (→75), BLE central scanner app (plan 67 terpisah).

---

## 2. WifiSettingsScreen

Pola: `ComponentScreen` + `ListView`/`ListItem` (sama gaya plan 60, lihat
`settings_screen.cpp` sebagai contoh struktur `build()`).

**Struktur layar (state-driven dari `wifi.state()`):**

```
WIFI                                  [TitleBar]
 ── Status: Connected  ▸ "MyAP" -52dBm 192.168.1.23   (atau "Off"/"Connecting…"/"Wrong password")
 ── [Toggle] WiFi Enabled
 ── Saved networks ──
     MyAP            ▸ (tap = connect / forget)
 ── Available ──        [Scan] (auto-scan saat masuk)
     ████ CoffeeShop  🔒
     ██   Office      🔒
     █    Open-Guest
```

**Interaksi:**
- Masuk layar → `tasks().submit(scan, done)` (worker), tampil "Scanning…", UI tetap responsif.
- Pilih SSID secured → buka **VirtualKeyboard** (mode password) → `saveNetwork` + `connect`
  di worker → status pindah `Connecting…` → `Connected`/`Wrong password` (dari `lastError()`).
- Pilih SSID open → connect langsung.
- Saved network row: tap = connect; long/secondary = forget (konfirmasi modal).
- Signal bars dari `WifiNetwork.rssi` (≥-50→4, ≥-60→3, ≥-70→2, else 1).
- Subscribe `WifiStateChanged`/`NetworkConnected/Disconnected` → `requestRedraw`.

**Edge cases (wajib, bukan happy-path saja):** scan kosong → "No networks found"; wrong
password → balik ke keyboard, bukan keluar; hardware fault (`available("net.wifi")==false`)
→ tampil "WiFi unavailable", semua aksi disabled.

## 3. BluetoothSettingsScreen

```
BLUETOOTH                             [TitleBar]
 ── Status: On · Advertising as "SkyRizz-E32"   (atau "Off")
 ── [Toggle] Bluetooth Enabled
 ── [Toggle] Discoverable (advertising)
 ── Paired devices ──
     iPhone          ▸ (tap = forget, konfirmasi)
```

- Toggle enable → `controller.enable(BtMode::Ble)` di worker (init stack berat).
- Pairing: subscribe `BtPairRequest` → tampil **modal numeric comparison** (passkey 6-digit)
  → tombol Confirm/Reject → `confirmPairing(accept)`.
- Bonded list dari `bondedCount()/bondedAt()`; forget → `forget(addr)` + konfirmasi.
- Liveness: `available("bt.ble")==false` → "Bluetooth unavailable".

> Catatan: BLE adapter juga dipakai RemoteService sebagai PLP cable. UI ini **hanya**
> mengelola radio/pairing/bond; otorisasi sesi remote diatur di plan 74. Pastikan toggle
> enable/disable di sini berkoordinasi dengan RemoteService (disable BLE = cabut cable).

## 4. Wiring

- `settings_screen.h`: tambah `Kind::Wifi`, `Kind::Bluetooth` + member `wifiSettings_`,
  `btSettings_`.
- `settings_screen.cpp build()`: section "Network" —
  ```cpp
  if (caps.has(caps::NetWifi)) items_.push_back({this, Wifi, "Wi-Fi"});
  if (caps.has(caps::BtBle))  items_.push_back({this, Bluetooth, "Bluetooth"});
  ```
  tempatkan setelah Controls (sebelum Touch), dan refleksikan state di accessory (mis.
  "Wi-Fi ▸ Connected").
- `launch()`: `case Wifi: navigate(wifiSettings_)`, `case Bluetooth: navigate(btSettings_)`.
- Target: tak perlu install app apa pun (screen hidup di Settings) — cukup pastikan
  `Esp32Platform` (skyrizz & dev-board sudah). Sim (`wasm` target) juga dapat otomatis via
  caps `SimWifiDriver`.

## 5. HW Verification — SkyRizz E32 (pindahan + perluasan plan 63)

**Prasyarat sdkconfig/board (hardening):**
- [ ] **Coexistence WiFi+BLE aktif** — `CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y` (S3 single radio
      2.4GHz; tanpa ini, BLE + WiFi scan bisa saling mematikan).
- [ ] NimBLE host enabled di `sdkconfig` skyrizz.
- [ ] `esp_crt_bundle` ter-include (HTTPS Ticker).
- [ ] Country code di-set (plan 72 §2.3).
- [ ] NVS partition ≥24KB (cek `partitions.csv`).
- [ ] Antena terpasang (PCB/u.FL) — dokumentasikan di `board_config.h`.

**Test cases (WiFi — dari plan 63, di-retarget ke `WifiSettingsScreen`):**

| TC | Skenario | Ekspektasi |
|---|---|---|
| W1 | Masuk Settings→Wi-Fi | Auto-scan, ≥1 AP, UI responsif (bisa Back) |
| W2 | Connect WPA2 (password benar) | `Connecting…`→`Connected`, IP + RSSI tampil |
| W3 | Password salah | `Wrong password`, balik ke keyboard, tak crash |
| W4 | Disconnect | `Idle`, `isOnline()==false` |
| W5 | Reboot | `autoConnect()` reconnect ke saved network |
| W6 | Ticker HTTP via WiFi | Harga BTC/USD tampil |

**Test cases (BLE):**

| TC | Skenario | Ekspektasi |
|---|---|---|
| B1 | Enable Bluetooth | Status `On · Advertising`, terlihat di scanner HP |
| B2 | Pair dari HP | Modal numeric muncul, Confirm → `Paired`, masuk bonded list |
| B3 | Forget device | Hilang dari bonded list, NVS bond terhapus |
| B4 | Disable Bluetooth | Advertising berhenti, RemoteService BLE cable lepas |

**Test cases (KOEKSISTENSI — yang paling berisiko di skyrizz):**

| TC | Skenario | Ekspektasi |
|---|---|---|
| C1 | BLE advertising **aktif**, lalu WiFi scan | Scan tetap dapat AP; BLE tak putus |
| C2 | WiFi connected, lalu pairing BLE | Keduanya jalan; tak ada reset/brownout |
| C3 | WiFi + BLE + kamera/LCD aktif | Tak ada crash PSRAM/heap; log bersih |

## 6. Tasks

- [ ] `WifiSettingsScreen` (.h/.cpp) — full state machine UI + keyboard + saved networks.
- [ ] `BluetoothSettingsScreen` (.h/.cpp) — enable/advertise/pairing modal/bonded list.
- [ ] `settings_screen`: section Network + gate caps + launch + accessory state.
- [ ] Verifikasi sim (`bun run forge:wasm`): WiFi router sim + BLE stub tampil & jalan.
- [ ] sdkconfig skyrizz: coexistence + NimBLE + crt bundle + country code.
- [ ] Build hijau 3 target.
- [ ] Flash skyrizz, jalankan W1–W6, B1–B4, C1–C3, catat hasil di `docs/plans/reports/`.

## 7. Acceptance criteria

- [ ] Menu **Wi-Fi & Bluetooth muncul** di Settings skyrizz-e32 (dan dev-board), digate caps.
- [ ] WiFi connect/disconnect/saved/auto-reconnect bekerja **di hardware** (W1–W6 pass).
- [ ] BLE enable/pair/forget bekerja **di hardware** (B1–B4 pass).
- [ ] **Koeksistensi C1–C3 pass** — WiFi & BLE jalan bersamaan tanpa crash/brownout.
- [ ] State liveness benar: cabut/fault radio → UI tampil "unavailable", bukan hang.
- [ ] Tak ada regresi: Dolphin/BadUsb/Home tetap jalan.
- [ ] STATE.md diupdate: WiFi/BLE skyrizz `build` → `HW-verified`.
