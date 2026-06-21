# 63 — WiFi Hardware Verification (SkyRizz E32)

> ⚠️ **SUPERSEDED oleh [plan 73](73-connectivity-settings-ui.md).** Plan ini merujuk
> `firmware/core/src/apps/wifi_app.cpp` yang **sudah dihapus** saat rewrite UI/Aether
> (commit `1230296`) — UI WiFi sekarang dibangun ulang sebagai screen di Settings. Semua
> test case di bawah dipindah & diperluas (WiFi + BLE + koeksistensi) ke plan 73 §5.
> Dibiarkan untuk arsip; **jangan dieksekusi langsung**.

> Prosedur test end-to-end WiFi di hardware SkyRizz E32. WiFi driver sudah
> build ✅ dan sim ✅, tapi belum pernah ditest di hardware nyata.

- Status: 🔴 Superseded (→ plan 73)
- Depends on: 20 (WiFi & Networking), 28 (SkyRizz E32), 23 (Keyboard + HTTP)
- Blocks: 62 (NTP), semua networked apps

---

## 1. Prasyarat Hardware

- SkyRizz E32 board ter-flash firmware terbaru
- WiFi AP 2.4 GHz dengan WPA2-PSK (channel 1–11)
- Kredensial diketahui: SSID + password
- Serial monitor terhubung (115200 baud)
- Log level: `debug` atau `trace` untuk WiFi component

## 2. Test Cases

### TC-01: WiFi Scan

| Step | Ekspektasi |
|------|-----------|
| Buka WifiApp dari launcher | Tampil "Scanning..." |
| Tunggu 1–3 detik | List SSID muncul (minimal 1 AP terdeteksi) |
| UI tetap responsif (bisa Cancel) | Tombol Cancel bekerja, kembali ke Overview |

**Verifikasi:** serial log `esp_wifi_scan_start` + `scan done, found N APs`.

### TC-02: Connect Open Network (jika tersedia)

| Step | Ekspektasi |
|------|-----------|
| Pilih SSID open (tanpa lock icon) | Tidak prompt password |
| Tap Connect | Status "Connecting..." → "Connected" |
| Cek status bar WiFi icon | Icon connected (misal: full bars) |

### TC-03: Connect WPA2-PSK

| Step | Ekspektasi |
|------|-----------|
| Pilih SSID WPA2 | Muncul keyboard untuk input password |
| Input password benar, tap Connect | Status "Connecting..." → "Connected" |
| Cek IP address tampil | IP valid (biasanya 192.168.x.x) |
| Cek RSSI tampil | Nilai negatif (misal: -45 dBm) |

### TC-04: Wrong Password

| Step | Ekspektasi |
|------|-----------|
| Pilih SSID WPA2, input password salah | Status "Connecting..." → "Wrong password" atau "Connection failed" |
| Tidak crash, bisa coba lagi | Kembali ke input password (bukan ke Overview) |

### TC-05: Disconnect

| Step | Ekspektasi |
|------|-----------|
| Dari Overview (connected state), pilih Disconnect | Status "Disconnected", WiFi icon hilang |
| Cek `isOnline() == false` | HTTP request gagal dengan error appropriate |

### TC-06: Auto-Reconnect After Reboot

| Step | Ekspektasi |
|------|-----------|
| Connect ke SSID (TC-03), pastikan sukses | Connected |
| Reboot device (power cycle atau reset pin) | Device boot |
| Tunggu 5–10 detik | Auto-reconnect ke SSID yang sama |
| Cek status bar | WiFi icon connected, IP tampil |

**Verifikasi:** kredensial ter-simpan di NVS (`nvs_get_str("wifi_ssid")`).

### TC-07: Scan Setelah Connected

| Step | Ekspektasi |
|------|-----------|
| Dalam state connected, buka WifiApp → Scan | Scan tetap jalan |
| List SSID tampil, AP yang sedang connected ditandai | Checkmark atau highlight |

### TC-08: Connect Hidden SSID

| Step | Ekspektasi |
|------|-----------|
| Pilih "Add Hidden Network" | Input SSID manual + password |
| Input SSID + password | Connect sukses |

### TC-09: No AP In Range

| Step | Ekspektasi |
|------|-----------|
| Device di ruangan tanpa WiFi | Scan result: list kosong |
| Tampilkan pesan "No networks found" | Tidak crash, bisa Cancel |

### TC-10: RSSI Display Accuracy

| Step | Ekspektasi |
|------|-----------|
| Dekatkan device ke AP (1 meter) | RSSI ≥ -40 dBm |
| Jauhkan device (10 meter, tembok) | RSSI menurun (≤ -60 dBm) |
| Cek icon bar sesuai RSSI | 4 bar → 3 bar → 2 bar → 1 bar |

### TC-11: TickerApp HTTP via WiFi

| Step | Ekspektasi |
|------|-----------|
| Connect WiFi (TC-03) | Connected |
| Buka TickerApp | HTTP GET ke Binance API |
| Tampilkan harga BTC/USD | Harga tampil, update periodik |

## 3. Regression Checklist

Setiap ada perubahan di WiFi driver, jalankan minimal:

- [ ] TC-01 (scan)
- [ ] TC-03 (connect WPA2)
- [ ] TC-05 (disconnect)
- [ ] TC-06 (auto-reconnect)

## 4. Known Potential Issues

1. **TLS cert bundle tidak ter-embed.** Cek `idf_component.yml` include
   `esp_crt_bundle`. Tanpa ini, HTTPS (TickerApp Binance) gagal.
2. **WiFi country code.** Set `"ID"` atau `"US"` supaya channel 1–11 bisa di-scan.
   Tanpa country code, ESP32 default ke channel 1–13 (mungkin ilegal di beberapa
   region).
3. **NVS partition.** Pastikan `partitions.csv` punya partisi `nvs` dengan size
   cukup (≥24KB). Tanpa NVS, kredensial tidak persistent.
4. **Antenna.** SkyRizz E32 mungkin punya antenna PCB atau u.FL. Pastikan
   antena terpasang.

## 5. Files

| File | Catatan |
|------|---------|
| `firmware/boards/skyrizz-e32/partitions.csv` | Cek NVS partition |
| `firmware/platforms/esp32/src/esp32_wifi_driver.cpp` | Driver yang ditest |
| `firmware/core/src/apps/wifi_app.cpp` | WifiApp UI |

## 6. Acceptance Criteria

- [ ] Semua test case TC-01 s/d TC-11 pass di hardware SkyRizz E32
- [ ] Tidak ada crash/hang selama test
- [ ] Serial log tidak ada error ESP-IDF WiFi (misal: `wifi: failed to ...`)
- [ ] Auto-reconnect berfungsi setelah power cycle
- [ ] `isOnline() == true` setelah connect, `false` setelah disconnect
