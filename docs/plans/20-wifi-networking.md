# 20 — WiFi & Networking (Nema App-Model Edition)

> WiFi end-to-end yang ditulis ULANG untuk arsitektur Nema kernel + app-model (plan 19.5/19.6). Versi lama plan ini dibuat untuk model cooperative lama (push screen ke ViewDispatcher, scan/connect blocking di UI) — sudah usang. Di sini WiFi = **app** (`WifiApp`) yang jalan di thread sendiri, scan/connect blocking di **TaskRunner worker**, UI tak pernah freeze.

- Status: ✅ Functionally complete (host+ESP32 build green). HAL extended, SimWifiDriver (router model) + Esp32WifiDriver (scan/connect/NVS auto-reconnect), WifiApp state machine on TaskRunner, ui::TextInput + VirtualKeyboard, web panel (now WiFiTab), credential persistence via ConfigStore (Plan 24). Sim scan→pick→password→connect verified. Remaining minor gap: "IP Settings" menu is display-only (no DHCP/static editor) — deferred, DHCP default works. HW final verification pending.
- Milestone: M7 (Connectivity)
- Depends on: **19.5 (Nema: Thread/MessageQueue/TaskRunner)**, **19.6 (App-model: IApp/AppHost/AppContext)**, 16 (ESP32 Platform), 17 (Dev Board)
- Blocks: OTA (plan 21), NTP sync, networked apps (Fase D plan 19.6)

> Kontrak kejujuran (sama seperti 19.5/19.6): tidak ada klaim ajaib; tiap fase build & jalan di host+ESP32; race ditangani by-design; verifikasi akhir di hardware.

---

## 0. Apa yang BERUBAH dari plan 20 lama (kenapa ditulis ulang)

| Aspek | Plan 20 lama (cooperative) | Plan 20 baru (Nema app-model) |
|---|---|---|
| WiFi UI | `WifiScreen` (IScreen) push ke ViewDispatcher | **`WifiApp`** (IApp) di thread sendiri via AppHost |
| Scan/connect | blocking call di UI loop → freeze | jalan di **TaskRunner worker** → UI tetap hidup |
| Input teks | `TextInputScreen` push/pop ViewDispatcher | overlay digambar app di buffer-nya (pola modal Counter) |
| Hasil scan async | event `WifiScanComplete` → screen subscribe EventBus | event tetap, TAPI di-drain via AsyncEventPoster → app baca hasil; atau callback TaskRunner |
| Cross-thread | belum ada model thread | semua lewat `nema::MessageQueue` / poster (aman) |

**Inti:** WiFi adalah kasus uji sempurna untuk pilar "jangan freeze" — scan WiFi nyata di ESP32 makan 1-3 detik. Di model lama itu membekukan layar; di model app, app thread submit ke worker dan tetap menggambar "Scanning…" sambil UI responsif.

---

## 1. Goal (tidak berubah, cara capai berubah)

Dari device, tanpa flash ulang:
1. Lihat status koneksi (SSID, IP, RSSI)
2. Scan jaringan tersedia (non-blocking)
3. Pilih jaringan → input password (overlay char-picker dalam app)
4. Connect ke hidden SSID (input SSID manual)
5. Disconnect
6. Lihat & ubah IP config (DHCP / static)

**Simulator:** inject network list + IP via web panel (tak perlu char-picker; web punya input). **Dev Board:** scan nyata via esp_wifi, connect+password, simpan kredensial NVS.

---

## 2. Arsitektur (app-model)

```
WifiApp (thread, via AppHost — Normal mode, status bar tampil)
  state machine: Overview → Scanning → PickNetwork → EnterPassword → Connecting → Result
  │
  ├─ scan():   ctx.runtime().tasks().submit(
  │               [drv]{ drv->scan(); },            // worker thread: blocking 1-3s
  │               [self]{ self->scanReady = true; } // UI thread flag (app polls)
  │            )  → app gambar "Scanning..." sambil tetap responsif/cancelable
  │
  ├─ connect(): submit ke worker juga (esp_wifi_connect blocking-ish)
  │
  ├─ input teks (password/SSID): overlay char-picker digambar app di buffer-nya
  │            (pola sama modal Counter — TIDAK push screen, no ViewDispatcher race)
  │
  └─ status/IP: query IWifiDriver (sudah thread-safe untuk read sederhana)
```

Tidak ada `WifiScreen`/`WifiConnectScreen`/`WifiIpScreen`/`TextInputScreen` sebagai IScreen lagi — semua jadi **state + overlay di dalam WifiApp**. Lebih sedikit objek, nol cross-thread UI race.

---

## 3. HAL Extension (`IWifiDriver`)

```cpp
struct WifiNetwork {
    char    ssid[33];
    int8_t  rssi;      // dBm
    bool    secured;
};

struct WifiIpConfig {
    bool dhcp = true;
    char ip[16]{}, mask[16]{}, gw[16]{};
};

struct IWifiDriver : IDriver {
    // signature diperluas — password opsional (default "")
    virtual bool        connect(const char* ssid, const char* password = "") = 0;
    virtual void        disconnect()  = 0;
    virtual bool        isConnected() const = 0;
    virtual const char* ssid()        const = 0;

    // baru — dipanggil dari TaskRunner worker (boleh blocking)
    virtual void                            scan() = 0;          // blocking; isi scanResults()
    virtual const std::vector<WifiNetwork>& scanResults() const = 0;
    virtual const char*                     ip()       const = 0;
    virtual WifiIpConfig                    ipConfig() const = 0;
    virtual void                            setIpConfig(const WifiIpConfig&) = 0;
};
```

**Catatan threading HAL (jujur):** `scan()`/`connect()` dipanggil dari **worker thread** (boleh blocking). `scanResults()`/`ip()`/`isConnected()` dibaca dari **app thread**. Karena scan menulis `scanResults_` lalu app membacanya SETELAH callback selesai (happens-before via MessageQueue), tidak perlu lock di driver. Aturan: app tidak baca `scanResults()` selagi `scan()` masih jalan — state machine menjamin ini (baca hanya di state PickNetwork, setelah callback).

---

## 4. Events

```cpp
inline constexpr const char* WifiScanComplete = "WifiScanComplete"; // {"count":"N"}
```
Tetap ada untuk konsumen lain (mis. status bar / log), TAPI WifiApp sendiri **tidak bergantung EventBus** untuk alur utama — ia pakai callback TaskRunner (lebih langsung, tetap di UI thread). Event diposting via `AsyncEventPoster` dari worker → aman.

---

## 5. Driver Implementations

### 5.1 SimWifiDriver
- Tambah `scanResults_`, `ip_`, `ipConfig_`.
- `scan()`: salin network list yang di-inject web panel → `scanResults_` (sim tak perlu delay; boleh `Thread::sleepMs(300)` untuk meniru latency biar UX "Scanning…" terlihat).
- `connect(ssid, pw)`: set connected, simpan ssid, set ip_ dummy (atau dari inject). Password diabaikan.
- Command baru web→sim: `wifi_set_networks`, `wifi_set_ip`.

### 5.2 Esp32WifiDriver
- `connect(ssid, pw)`: isi `wifi_config_t.sta.password`, `esp_wifi_connect()`.
- `scan()`: `esp_wifi_scan_start(NULL, true)` **blocking** (true = block sampai selesai) — OK karena dipanggil di worker thread, bukan UI. Lalu `esp_wifi_scan_get_ap_records()` → `scanResults_`.
- `ip()`: `esp_netif_get_ip_info` → format.
- NVS: simpan ssid+pw saat connect sukses; auto-reconnect saat `start()` bila ada.
- Event tetap via `AsyncEventPoster` (sudah dari 19.5).

---

## 6. WifiApp (state machine)

```cpp
class WifiApp : public IApp {
    const char* id()   const override { return "com.palanu.wifi"; }
    const char* name() const override { return "WiFi"; }
    // Normal mode (status bar tampil) — default fullscreen()==false
    void run(AppContext& ctx) override;
};
```

State: `Overview, Scanning, PickNetwork, EnterText, Connecting, Result`.
- **Overview**: status (SSID/IP/connected) + menu (Scan / Hidden SSID / Disconnect / IP Settings).
- **Scanning**: submit `scan()` ke worker; gambar "Scanning…"; saat callback set flag → PickNetwork.
- **PickNetwork**: list `scanResults()` (RSSI + gembok); pilih → EnterText (kalau secured) atau langsung Connecting.
- **EnterText**: overlay char-picker (helper baru `ui::TextInput` — lihat §7) untuk password/SSID.
- **Connecting**: submit `connect()` ke worker; gambar "Connecting…"; callback → Result.
- **Result**: sukses/gagal → kembali Overview.

Semua menggambar di `ctx.canvas()` + `ctx.present()`. Input via `ctx.waitInput()`. Kerja berat via `ctx.runtime().tasks().submit()`.

---

## 7. Char-Picker reusable (`ui::TextInput`)

Karena bukan lagi IScreen, jadikan **helper state kecil** yang app pakai (DRY untuk app lain: WiFi, nanti login form, dll):

```cpp
namespace nema::ui {
struct TextInput {
    char     buf[64] = {};
    uint8_t  len     = 0;
    int      charIdx = 0;        // posisi di charset
    void handle(Key k, bool& done, bool& cancel);  // Up/Down pilih, Right append, Left hapus, Select submit, Cancel batal
    void draw(Canvas& c, const char* prompt);       // overlay di buffer app
};
}
```
Charset: a-z A-Z 0-9 simbol + `[DONE]`. Kontrol 6-tombol persis draft lama, tapi tanpa push/pop screen.

---

## 8. Settings integration

`SettingsScreen` item "WiFi" (sudah ada, muncul bila `capabilities().has("wifi")`) → `onSelect`: launch `WifiApp` via AppHost (pola sama plugin lain). SettingsScreen tetap system-screen; WiFi jadi app.

---

## 9. Web Panel (simulator) — "router" interaktif

> **REVISI (implementasi final):** panel BUKAN inject daftar jadi. Panel = **router**: kelola
> network yang "ada di sekitar". Device menjalani alur asli (scan → pilih → ketik password →
> connect). Ini menjamuk "scan" punya arti dan password divalidasi.

`ControlsPanel.tsx` WiFi section:
- Daftar network, tiap baris punya: **SSID + password + RSSI (slider) + toggle online/offline** + hapus.
- Tombol "+ Add network". Tiap perubahan → `{"cmd":"wifi_set_networks","networks":[{ssid,password,rssi,online},...]}`.
- Status koneksi live dari event `NetworkConnected/Disconnected` (track di `useSimSocket.ts`).

Perilaku driver (seperti router nyata, terverifikasi 4 skenario):
- Password salah → `connect()` gagal (auth fail), tak terhubung.
- Connect ke network **offline** → terhubung TAPI HTTP gagal (`SimHttpClient` di-gate ke `isOnline()`).
- Network online + password benar → terhubung + internet jalan.

> `SimHttpClient` (curl) **wajib** dicek terhadap status WiFi simulasi (`isOnline()`), kalau tidak
> Ticker akan tetap fetch via internet Mac host walau WiFi "disconnect" — tidak jujur.

Command lama `wifi_set_ip` di-drop (IP otomatis saat connect). Helper test `wifi_connect{ssid,password}`
tetap ada untuk skrip CLI.

---

## 10. File Plan

| File | Aksi |
|---|---|
| `core/include/palanu/hal/wifi.h` | extend (WifiNetwork, WifiIpConfig, scan/ip/ipConfig) |
| `core/include/palanu/event/event.h` | +WifiScanComplete |
| `core/include/palanu/ui/text_input.h` (+.cpp) | **baru** — char-picker helper |
| `core/include/palanu/apps/wifi_app.h` (+.cpp) | **baru** — WifiApp state machine |
| `core/include/palanu/plugins/wifi_plugin.h` (+.cpp) | **baru** — thin launcher (atau langsung dari Settings) |
| `core/src/screens/settings_screen.cpp` | WiFi → launch WifiApp |
| `core/CMakeLists.txt` | +sources baru |
| `platforms/simulator/.../sim_wifi_driver.{h,cpp}` | scan/ip/ipConfig + state inject |
| `platforms/simulator/src/command_reader.cpp` | wifi_set_networks, wifi_set_ip |
| `platforms/esp32/.../esp32_wifi_driver.{h,cpp}` | password, scan, ip, NVS |
| `packages/simulator/components/ControlsPanel.tsx` | WiFi UI baru |
| `packages/simulator/lib/useSimSocket.ts` | track wifi state |

---

## 11. Fase Implementasi

1. **HAL + events** — extend wifi.h, +WifiScanComplete. Build dua target (driver lama belum implement method baru → beri stub `=default`/kosong dulu agar kompilasi).
2. **SimWifiDriver** — implement scan/ip/ipConfig + command inject. Test via web/CLI.
3. **ui::TextInput** — char-picker helper + unit-ish test di app dummy.
4. **WifiApp** — state machine pakai TaskRunner. Launch dari Settings. Test di simulator (inject networks → scan → pick → password → connect).
5. **Esp32WifiDriver** — password + scan nyata + ip + NVS. Flash & tes hardware.
6. **Web panel** — ControlsPanel + useSimSocket.

Tiap fase: build host+ESP32, smoke test, baru lanjut.

---

## 12. Acceptance Criteria

**Simulator**
- [ ] Inject network list dari web → app Scan menampilkannya
- [ ] Pilih secured network → char-picker → connect (UI tak freeze saat "Scanning/Connecting")
- [ ] Set IP (DHCP/static) dari web → IP Settings menampilkannya
- [ ] Status bar tetap tampil di WifiApp (Normal mode)

**Dev Board**
- [ ] Scan nyata 1-3s → UI tetap responsif (clock jalan, Cancel bekerja) selama scan
- [ ] Connect dengan password via char-picker → dapat IP DHCP
- [ ] Auto-reconnect dari NVS saat boot
- [ ] Disconnect bekerja

**Cross-cutting**
- [ ] `scan()`/`connect()` jalan di TaskRunner worker, BUKAN UI thread (verifikasi: UI frame terus keluar selama operasi)
- [ ] Tidak ada akses Canvas/ViewDispatcher dari worker thread (race-free)
- [ ] ui::TextInput reusable (SSID & password pakai helper sama)

---

## 13. Non-Goals
- WPA-Enterprise/EAP, hotspot/AP mode, IPv6, DNS manual — future.
- Multiple saved networks (profiles) — future; v1 simpan 1 kredensial NVS.
- Captive portal handling.

---

## 14. Catatan Threading (ringkas, penting)
- **Worker (TaskRunner):** scan(), connect() — boleh blocking penuh.
- **App thread (WifiApp::run):** state machine, draw, input. Submit ke worker, poll flag/terima callback.
- **GUI thread:** composit frame WifiApp (via AppHost) + status bar.
- **Driver event (sys_evt ESP32):** → AsyncEventPoster → main task.
- Tidak ada satupun jalur di mana operasi WiFi blocking menyentuh GUI/app loop. Itu inti revisi ini.
