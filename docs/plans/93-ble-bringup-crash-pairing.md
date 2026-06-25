# 93 — BLE Bring-up: Fix Crash Controller, Pairing dari HP & Parity Flipper

> **Riset + rencana.** User minta "riset BLE/Bluetooth" untuk bikin sistem yang bisa
> dikontrol dari Settings seperti WiFi (enable, pairing, lupakan device), dengan tujuan
> awal **remote protocol — konek lewat HP dulu**, mengacu ke Flipper Zero Momentum.
>
> **Temuan riset:** sistemnya **sudah ~90% terbangun** (Plan 34 BLE/NimBLE, Plan 35
> PLP-over-BLE, Plan 73 BluetoothSettingsScreen, Plan 74 auth) **termasuk sisi HP**
> (Forge web app `packages/forge/` konek via **Web Bluetooth**). Arsitekturnya sudah
> meniru Flipper (GAP pairing → GATT serial → transport → dispatch). Yang menghalangi:
> **toggle Bluetooth ON di skyrizz-e32 → device force-restart** (panic di dalam
> `esp_bt_controller_init` ESP-IDF). Plan ini **bukan bikin dari nol** — ini
> **menghidupkan + memperbaiki crash + verifikasi di HW + menyamakan UX ke Flipper.**

- Status: 🟡 In progress (riset selesai, akar crash teridentifikasi)
- Milestone: M9+ (Remote/Forge maturation)
- Depends on: **34 (BLE/NimBLE)**, **35 (PLP/RemoteService)**, 73 (Connectivity Settings UI), 74 (auth), 88 (PLP v2)
- Blocks: demo "konek device dari HP", verifikasi HW Plan 73 §5 (WiFi + BLE coexistence)
- Catatan: Plan 73 §0 sudah **usang** — bilang screen WiFi/BT "hilang", padahal sekarang
  `bluetooth_settings_screen.cpp` + entri Settings sudah ada & ter-wire. Plan ini menggantikan
  bagian "BLE bring-up + HW verify" dari Plan 73.

---

## 0. Apa yang SUDAH ada (peta sistem)

### Sisi device (firmware)
| Lapisan | File | Status |
|---|---|---|
| HAL | `firmware/core/include/nema/hal/bluetooth.h` — `IBluetoothController` (radio on/off, nama device) + `IBleAdapter` (advertise, GATT, pairing, bonding, scan/connect Plan 67) | ✅ |
| Stack NimBLE | `firmware/platforms/esp32/src/esp32_ble.cpp` — peripheral + central, LE Secure Connections + numeric-comparison, bond persist NVS | ✅ |
| Transport remote | `firmware/core/include/nema/link/plp_ble.h` + `ble_link_transport.h` — GATT service `a7b30001-…`, TX `a7b30002` (notify, device→HP), RX `a7b30003` (write, HP→device) | ✅ |
| Codec PLP | `plp_codec.h` — frame `[0xAB][channel][flags][len:2][payload][crc8]`, channel Screen/Input/Log/Cli/File/Ota/System | ✅ |
| **Settings → Bluetooth** | `firmware/core/src/screens/bluetooth_settings_screen.cpp` — toggle on/off, "Discoverable", modal pairing (Confirm/Reject passkey), daftar paired + "forget" | ✅ |
| Auth | `firmware/core/include/nema/services/remote_auth.h` — password + session token, channel privileged wajib auth | ✅ |
| Wiring target | `esp32_platform.cpp:48` `ble_.onRegister(rt)`; `:77-78` `cable_.init(ble_)` + `mux_.add(&cable_)` | ✅ |

### Sisi HP (sudah ada juga)
`packages/forge/` route `/remote` — `BleTransport` pakai
`navigator.bluetooth.requestDevice({filters:[{services:[PLP_SERVICE]}]})`, plus opsi
USB-Serial & WebSocket. Setelah konek: mirror layar, kirim tombol, terminal CLI, file
browser, OTA. Kontrak UUID dibagi di `packages/link/src/uuids.ts`. **Target "konek lewat
HP" sudah terimplementasi end-to-end** — tinggal terbukti di hardware.

---

## 1. Crash: akar masalah (BLOCKER UTAMA)

> ✅ **RESOLVED (2026-06-25).** Akar sebenarnya: **arduino-esp32 `initArduino()` memanggil
> `esp_bt_controller_mem_release(ESP_BT_MODE_BTDM)` di boot** (guard `if (!btInUse())`), dan
> `btInUse()` itu weak-symbol default `false`. Memori controller dilepas → `esp_bt_controller_init()`
> kita jalan di memori bebas → crash di `btdm_controller_init`. **Fix: override strong
> `extern "C" bool btInUse(){ return true; }`** (`esp32_ble.cpp`). Lihat **ADR 0013**. BLE init
> dijadikan **on-demand** (bukan boot) supaya ~30 KB internal-nya tidak mencekik DMA SD/apps di boot.
> Semua teori di bawah (RAM, fragmentasi, urutan WiFi, BLE 5.0, coexistence) **terbukti BUKAN
> penyebab** — disimpan sebagai catatan investigasi.

### Gejala
Toggle Bluetooth ON di skyrizz-e32 → `Guru Meditation Error: Core 0 panic'ed (LoadProhibited)` → reboot.

### Backtrace (yang penting)
```
esp_bt_controller_init        (esp-idf .../controller/esp32c3/bt.c:1906)   ← S3 pakai controller C3/S3 shared (BLE-only)
  → btdm_controller_init      (arch_main.o)                                ← GAGAL di tengah
    → btdm_controller_deinit_internal                                      ← rollback otomatis
      → semphr_delete_wrapper (bt.c:940)  → strlen  LoadProhibited @0x33   ← dereference handle sampah
nimble_port_init              (nimble_port.c:292)
nema::Esp32Ble::enable        (esp32_ble.cpp:115)
BluetoothSettingsScreen::setEnabled lambda (bluetooth_settings_screen.cpp:54, di TaskRunner worker)
```

### Diagnosis
Crash **bukan di kode kita** — terjadi **di dalam `esp_bt_controller_init()` ESP-IDF**.
Controller mulai init (log `BLE_INIT: Using main XTAL as clock source`), lalu
`btdm_controller_init` **gagal**, memanggil rollback `deinit`-nya sendiri, dan rollback itu
men-dereference handle semaphore yang belum sempat dibuat (`0x33`) → LoadProhibited. Crash di
`semphr_delete` hanyalah **gejala**; penyakitnya = **`btdm_controller_init` gagal**.

### Akar paling mungkin: RAM internal habis
Controller BLE ESP32-S3 butuh blok **RAM internal (DRAM, bukan PSRAM)** untuk struktur
ISR/DMA — **tidak bisa** dipindah ke PSRAM (sama seperti WiFi RX DMA). Bukti:
1. Crash terjadi di **uptime ~68 menit** (`I (4084543)`), saat WiFi STA + LVGL framebuffer
   sudah memakan RAM internal → controller tak dapat blok yang dibutuhkan.
2. Tidak ada `esp_bt_controller_mem_release` di kode kita (sudah dicek) → **bukan** kasus
   "memori controller terlanjur dilepas".
3. Internal SRAM board ini memang ketat (~230 KB, satu-satunya RAM DMA-capable) dibagi
   WiFi + SD-DMA + LCD + camera + audio. Tim sudah "diet" WiFi (Plan 88: dynamic TX → PSRAM,
   trim RX 16→10, BA window 16→6). Menambah BLE controller di atasnya menyentuh batas.

### Hipotesis berurut (uji untuk konfirmasi)
1. **RAM internal habis (kuat).** Uji: toggle BLE **segera setelah boot** (RAM masih lega) vs
   setelah lama — kalau sukses saat fresh, ini terkonfirmasi.
2. **Coexistence assert WiFi+BLE.** Uji: matikan WiFi dulu, baru toggle BLE — kalau sukses
   tanpa WiFi, coexistence/memori-bersama penyebabnya.
3. **Fragmentasi (varian dari #1).** `min free heap` rendah setelah sesi panjang.

### Catatan tambahan
- `CONFIG_ESP_COREDUMP_ENABLE_TO_NONE=y` → crash **tidak terekam** ke partisi `coredump`
  (padahal partisinya ada di `partitions.csv`). Untuk kasus sulit, set
  `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y` agar bisa di-`esp-coredump` belakangan. Untuk
  sekarang panic backtrace di `idf.py monitor` sudah cukup.

### ⚠️ Pendekatan yang DITOLAK (jangan ulangi)
`CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y` **kontraproduktif** di board ini — IDF Kconfig
membuatnya mutually-exclusive dengan dynamic-TX-buffer, sehingga memaksa 16 static WiFi TX
buffer (~26 KB) **kembali ke RAM internal** → memperparah. Lihat komentar di
`sdkconfig.defaults` (baris ~36-42). WiFi diet (dynamic TX) sudah membebaskan ~35-40 KB,
lebih banyak dari yang bisa diselamatkan try-allocate.

### Rencana fix — Fase A ✅ (dikerjakan)
- [x] **Diagnostik + guard pre-flight** di `Esp32Ble::enable()` (`esp32_ble.cpp`): log
      `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` + `largest_free_block`; kalau free
      internal < ~50 KB → `log_->error` + `caps_->setState(BtBle, Fault)` + return false.
      **Mengubah crash→graceful "Bluetooth unavailable"**, sekaligus mengukur RAM saat itu.
      Ambang 50 KB **tunable** dari pembacaan device pertama.
- [x] **Fix utama (lever PSRAM yang benar):** `CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL=y`
      di `sdkconfig.defaults` → NimBLE host pool (ACL/HCI/GATT, ~10-25 KB) pindah ke PSRAM,
      melegakan RAM internal untuk controller (controller tetap internal). **Clean build
      wajib** (`rm -rf build && bun run build:skyrizz-e32`).
- [x] **Ukur pertama di HW:** free internal = **23 KB**, largest block = **14.8 KB** →
      controller gagal (butuh ~30-40 KB internal). NimBLE-host→PSRAM saja TIDAK cukup karena
      baseline internal sudah terlalu mepet. Guard bekerja: "Bluetooth unavailable", **tanpa
      reboot.**
- [x] **Diet RAM internal (Plan 93 iterasi 2):**
      - `chunkbuf_` LCD staging 32→16 baris (`lcd_driver.cpp`) → hemat ~10 KB internal
        statik (ref obs 10254: 20.5 KB reducible). Flush layar sedikit lebih lambat.
      - `CONFIG_BT_CTRL_BLE_MAX_ACT` 6→3 → kecilkan footprint controller (cuma butuh
        advertise + 1 koneksi HP).
      - Guard floor 50→28 KB agar controller benar-benar mencoba init di level RAM baru.
- [x] **Ukur ke-2 (WiFi off):** free=30 KB **tapi largest block=11 KB → tetap crash.**
      Penyakit sebenarnya = **FRAGMENTASI**, bukan total free. Guard di-`free` salah; harus
      di **blok kontigu**. Guard diperbaiki: gate `largestBlock >= 24 KB` & `free >= 40 KB`.

### Iterasi 3 — keputusan arsitektur: init BLE saat boot (Plan 93)
Akar: controller butuh **satu blok kontigu ~30 KB** yang **tak pernah ada saat runtime**
(heap sudah terfragmentasi oleh semua driver yang start eager di boot). Flipper tak kena ini
karena BLE-nya **chip terpisah**. Solusi deterministik di ESP32-S3 single-die:
- [x] **Init BLE controller di `Esp32Ble::start()` (boot)** — saat heap masih segar & ada blok
      kontigu besar. Toggle "Bluetooth" sekarang cuma start/stop **advertising**, bukan
      init/deinit controller. `nimble_port_init` tak pernah lagi jalan di heap terfragmentasi
      → **crash/reboot hilang total**. (`esp32_ble.cpp`: `initStack()` + `start()`; `enable()`
      jadi ringan; `disable()` cuma stop-advertise, controller tetap up.)
- [x] **Lazy kamera** — hapus `registerService(&camera_)` (`skyrizz_e32.cpp`). Kamera tak
      dipakai app manapun (cuma `count()`/`desc()` di settings), jadi DVP DMA-nya murni mubazir.
      Bebaskan ~8 KB internal + kurangi fragmentasi. App kamera masa depan panggil `start()` sendiri.
- [x] **chunkbuf 16→8** (~5 KB) + **WiFi static RX 10→6** (~6 KB) → ruang untuk BLE-at-boot.
- [ ] **Verifikasi HW:** flash, cek log boot `BLE stack up (controller reserved at boot)`,
      device boot tanpa boot-loop, toggle BLE ON → advertise tanpa crash, HP bisa lihat/pair.
      Bila boot-loop / alloc gagal → lazy mic juga (rework I2S speaker), atau radio-exclusivity.
- [ ] **Robustness re-enable:** toggle ON↔OFF berulang, dengan & tanpa WiFi aktif, tanpa
      reboot. Pastikan `disable()` (`nimble_port_stop`+`nimble_port_deinit`) tidak crash saat
      OFF→ON→OFF→ON.

---

## 2. Flipper Momentum — perbandingan & gap

`refs/flipper-zero-momentum-firmware/`. Arsitektur kita sudah sejajar; ini gap-nya.

| Aspek | Flipper Momentum | kairo (sekarang) | Gap |
|---|---|---|---|
| GATT service | "Serial service" `fe60…` | PLP service `a7b30001…` | — |
| Karakteristik | RX(write) + TX(indicate) + **FlowControl(notify)** + **RPC-status** | RX(write) + TX(notify) | **Tidak ada flow control** |
| Backpressure | FlowControl char umumkan ruang buffer RX | `mtu()` fix 180, tanpa backpressure | Frame bisa drop saat streaming layar |
| Framing | raw chunk → RPC protobuf | PLP codec (channel + crc8) | — (PLP setara) |
| Pairing | GAP PIN-show / numeric comparison → bond `bt.keys` | LE SC numeric comparison → bond NVS | — (setara) |
| Advertising | service UUID **16-bit** + appearance + nama | hanya **128-bit** UUID + nama | Discovery lebih lambat/berat di sebagian HP |
| Settings UX | toggle + "Unpair All Devices" | toggle + "Discoverable" + forget per-device | kairo lebih lengkap ✅ |
| App HP | Flipper mobile (protobuf RPC) | Forge web (PLP, Web Bluetooth) | — |

Referensi file Flipper: `targets/f7/ble_glue/services/serial_service.c`, `.../gap.c`,
`applications/services/bt/bt_service/bt.c`, `applications/services/rpc/rpc.c`.

---

## 3. Verifikasi pairing dari HP — Fase B (setelah crash beres)

Tujuan utama user: "konek lewat HP dulu". Skenario uji di hardware skyrizz-e32:
- [ ] **Advertise & discover.** Toggle BLE ON → device muncul sebagai "Palanu" (atau nama
      profil) saat scan dari Chrome/Edge (Web Bluetooth) atau scanner BLE Android.
- [ ] **Pairing numeric comparison.** Forge `/remote` → "Bluetooth (BLE) → Scan…" → pilih
      device → passkey muncul di **layar device** (modal `pendingPair_`) → Confirm di device →
      bonded (`events::BtPaired`).
- [ ] **Sesi remote jalan.** Setelah konek: mirror layar (channel Screen, RLE), kirim tombol
      (channel Input), log streaming. Cek `BleLinkTransport::send` → `notify(CHAR_TX)`.
- [ ] **Bond persist.** Reboot device → reconnect tanpa pairing ulang (bond di NVS).
- [ ] **Forget.** Settings → Bluetooth → paired device → "forget" → `ble_gap_unpair`.
- [ ] **Coexistence.** WiFi ON + BLE ON bersamaan, streaming remote sambil WiFi konek —
      tidak reboot, tidak putus. (Ini target lama Plan 73 §5.)

---

## 4. Parity Flipper — Fase C (polish, boleh nyusul)

- [ ] **Flow control characteristic** (ala Flipper) di PLP GATT — umumkan ruang buffer RX
      device agar HP tidak overrun saat bulk/screen-stream. Selaraskan dengan PLP v2 (Plan 88).
- [ ] **Advertising 16-bit service UUID + appearance** untuk discovery lebih cepat.
- [ ] **Nama device dari profil** dipastikan ter-set sebelum advertise
      (`esp32_platform.cpp:118-119` set device name dari `profile_.deviceName()`).
- [ ] **Status bar BLE icon** — `StatusBarData.ble` sudah ada; wire liveness `available(BtBle)`
      seperti WiFi icon.

---

## 5. Definition of Done

1. **STATE.md** — baris Bluetooth/BLE: `build` → `HW-verified` setelah Fase B lulus.
2. **docs/feats/** — buat `docs/feats/bluetooth-remote.md` (cara kerja pairing + remote BLE
   sekarang), atau perluas `remote-desktop.md`.
3. **docs/decisions/** — ADR: NimBLE host → PSRAM (`MEM_ALLOC_MODE_EXTERNAL`) + guard heap
   pre-flight; catat kenapa `SPIRAM_TRY_ALLOCATE_WIFI_LWIP` ditolak (cegah reboot saat
   controller kehabisan RAM internal).
4. **Checklist** plan ini dicentang seiring kerjaan.
5. **Commit** conventional (`fix(ble):`, `feat(ble):`, `docs:`).
