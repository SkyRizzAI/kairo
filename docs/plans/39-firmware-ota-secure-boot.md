# 39 — Firmware OTA: Secure Boot + Signed Update + A/B Rollback

> Update **seluruh image firmware** lewat jaringan (WiFi pull dari Forge **atau**
> push lewat PLP) tanpa kabel/esptool — dengan **aman**: image ditandatangani &
> diverifikasi, dua slot app (A/B) supaya update **atomic**, dan **rollback otomatis**
> kalau image baru gagal boot. Plus **settings terenkripsi** untuk rahasia.
>
> **Bedakan dengan Plan 37 (App OTA):** Plan 37 mengirim **satu app `.kapp`** ke
> device hidup (OS tetap). Plan 39 ini mengganti **firmware itu sendiri** (kernel +
> driver + app bawaan). Dua-duanya "OTA" tapi beda lapisan. Lihat §0.1.
>
> Asal-usul: rekomendasi **P5/P6** di `docs/research/akiraos-vs-palanu.md` (pola
> AkiraOS: MCUboot dual-slot + SHA-256 + signature + rollback; settings AES-GCM).
> AkiraOS pakai MCUboot (Zephyr); kita pakai padanannya di **ESP-IDF**:
> `esp_https_ota`/`app_update` + Secure Boot v2 — **tanpa** menarik Zephyr.

- Status: 📝 **PLANNED** (2026-06-08). Belum mulai.
- Milestone: M11 (Field Reliability / Production).
- Depends on: **16 (ESP32 Platform)**, **20 (WiFi)**, **34/35 (PLP transport —
  Channel::Ota sudah ada)**, **36 (Forge — registry `firmware.*` + signing)**,
  **24 (Config Store — untuk settings terenkripsi)**.
- Blocks: rilis produksi / update di lapangan tanpa bongkar device.

---

## 0. Konsep & keputusan

### 0.1 Dua jenis "OTA" (penting — ini jawaban "OTA itu apa")
**OTA = Over-The-Air**: meng-update software di device lewat nirkabel/koneksi,
**tanpa** colok kabel & flash manual (esptool). Di Palanu ada **dua lapis**:

| | **App OTA** (Plan 37 Fase 6) | **Firmware OTA** (Plan 39 ini) |
|---|---|---|
| Yang di-update | satu **app `.kapp`** (JS bundle) | **seluruh image firmware** (`.bin`) |
| Ukuran | KB | MB |
| OS jalan terus? | ✅ ya, OS tak berubah | ❌ perlu reboot ke image baru |
| Risiko nge-brick | rendah (app di-sandbox) | tinggi → butuh secure-boot + rollback |
| Mekanisme | `IAppStore.install()` simpan ke FS | tulis ke partisi OTA kedua + swap + verify |
| Transport | PLP `Channel::Ext`/App, atau HTTP | PLP `Channel::Ota` (sudah ada) / `esp_https_ota` |

> Saat ini `/flash` (Forge, Plan 36) menulis **raw bin via kabel (Web Serial /
> esptool-js)** — itu **bukan OTA**, itu flash kabel. Plan 39 menambah jalur
> **nirkabel + aman**.

### 0.2 Partisi: factory tunggal → dual-slot OTA
`partitions.csv` skyrizz sekarang: satu `factory` app (0x500000). Untuk A/B kita
ganti jadi `ota_0` + `ota_1` + `otadata`:
```
# usulan (16MB):
otadata,  data, ota,     0xe000,   0x2000,
ota_0,    app,  ota_0,   0x10000,  0x500000,
ota_1,    app,  ota_1,   0x510000, 0x500000,
storage,  data, littlefs,0xA10000, 0x5E0000,   # geser dari spiffs (Plan 38)
coredump, data, coredump,0xFF0000, 0x10000,
```
> Trade-off: storage app (Plan 38) menyusut dari ~11MB → ~6MB karena ada dua slot
> app 5MB. Masih cukup besar. **Koordinasi wajib dengan Plan 38** soal layout ini.

### 0.3 Secure Boot v2 + signed image (ESP-IDF, bukan MCUboot)
- **Secure Boot v2**: bootloader memverifikasi **tanda tangan RSA-3072/ECDSA** app
  sebelum boot. Kunci privat disimpan di Forge/CI, publik di e-fuse device.
- **App rollback** (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`): image baru di-mark
  `PENDING_VERIFY`; kalau app tak panggil `esp_ota_mark_app_valid_cancel_rollback()`
  setelah self-test, bootloader **otomatis balik** ke slot lama saat reboot.
- (Opsional) **Flash encryption** — catatan, tidak wajib v1.

### 0.4 Dua jalur pengiriman (pilih, bisa dua-duanya)
1. **Pull (WiFi)**: device tanya registry Forge (`firmware.version`/`list` — Plan 36
   sudah ada) → kalau ada versi baru → `esp_https_ota` tarik `.bin` (HTTPS).
2. **Push (PLP)**: Forge kirim `.bin` chunk lewat **`Channel::Ota`** (sudah
   ter-reserve di `klp_codec.h`) → `RemoteService` tulis ke partisi OTA. Berguna
   lewat BLE/USB/virtual-cable tanpa WiFi.

---

## 1. Goal (acceptance tingkat-tinggi)
1. **`IOtaUpdater`** (core, transport-agnostic): `begin(totalSize)`, `write(chunk)`,
   `end()` → verifikasi SHA-256 (+ signature di bootloader) → set boot slot.
2. **Dual-slot A/B** aktif (partisi + otadata). Update **atomic**; gagal di tengah
   tak merusak slot aktif.
3. **Rollback otomatis**: image baru yang gagal self-test → device balik ke slot lama.
4. **Pull (WiFi)** dari Forge registry **dan** **Push (PLP `Channel::Ota`)** keduanya
   mengisi `IOtaUpdater`.
5. **Forge**: `firmware.*` tRPC menambah versi + **tanda tangan**; `publish-firmware.sh`
   menandatangani build; halaman `/flash` menambah tab **"OTA (wireless)"**.
6. **Settings terenkripsi** (AES-256-GCM via mbedtls) untuk value sensitif (wifi psk,
   token) di `IConfigStore`.
7. Aman di-test: build hijau; verifikasi penuh butuh device (flash sekali via kabel
   untuk menanam slot + kunci, sisanya OTA).

---

## 2. Arsitektur

### 2.1 Core
```
firmware/core/include/palanu/ota/ota_updater.h   // IOtaUpdater (begin/write/end/abort)
firmware/core/include/palanu/ota/ota_session.h   // state, progress, hash akumulasi
firmware/core/src/ota/...                        // verifikasi SHA-256, progress event
```
- `IOtaUpdater` transport-agnostic: dipanggil oleh **OTA-pull** (WiFi) maupun
  **RemoteService** (PLP `Channel::Ota`). Mirip pola `ILinkTransport` kita.
- Emit `events::OtaProgress`/`OtaDone`/`OtaFailed` ke EventBus → UI bisa tampil bar.

### 2.2 esp32 backend
```
firmware/platforms/esp32/.../esp32_ota.{h,cpp}   // esp_ota_begin/write/end, set_boot_partition
```
- Implement `IOtaUpdater` pakai `esp_ota_*`. `esp_ota_get_next_update_partition()`,
  tulis chunk, `esp_ota_end()` (verifikasi), `esp_ota_set_boot_partition()`.
- Boot baru: `app_main` jalankan **self-test ringan** → `esp_ota_mark_app_valid_...`.
  Kalau crash sebelum mark → rollback otomatis.

### 2.3 Transport masuk
- **PLP**: `RemoteService` (Plan 35) menangani `Channel::Ota` → forward chunk ke
  `IOtaUpdater`. Tambah opcode `OtaOp{Begin,Data,End,Abort}`.
- **WiFi pull**: `OtaPullService` (IService) — cek registry Forge periodik/manual →
  `esp_https_ota` (atau manual stream → `IOtaUpdater`).

### 2.4 UI device
- Settings → **System → Update**: tampil versi sekarang vs tersedia, tombol
  "Check"/"Update", progress bar (dari event), status rollback.

### 2.5 Forge (Plan 36 extend)
- `firmware.*`: tambah `version` + `signature` + `notes` di manifest
  (`publish-firmware.sh` menandatangani `.bin` dengan kunci CI).
- `/flash`: tab **OTA** → pilih device (PLP session) → push `.bin` via `Channel::Ota`
  dengan progress (reuse RemoteSession).

### 2.6 Settings terenkripsi (P6)
```
firmware/core/.../config/encrypted_config.{h,cpp}  // wrapper IConfigStore + AES-GCM
```
- Value dengan flag `secret` → AES-256-GCM (mbedtls; kunci dari e-fuse/derive).
  Magic + IV(12) + tag(16) + ciphertext. Key non-secret tetap plaintext (cepat).

---

## 3. Fase implementasi

| Fase | Isi | Tes |
|---|---|---|
| **0. Partisi A/B** | `partitions.csv` → ota_0/ota_1/otadata (+ koord. Plan 38 storage) | esp32 build hijau; boot dari ota_0 |
| **1. IOtaUpdater + esp32** | `esp_ota_*` backend; SHA-256 verify; progress event | device: tulis image valid → set boot → reboot ke image baru |
| **2. Rollback** | `app valid`/self-test + `BOOTLOADER_APP_ROLLBACK_ENABLE` | device: image "rusak" sengaja → auto-rollback ke slot lama |
| **3. Push via PLP** | `Channel::Ota` opcodes; RemoteService→IOtaUpdater; Forge push UI | Forge→device (BLE/USB/sim): update tanpa WiFi |
| **4. Pull via WiFi** | `OtaPullService` cek registry Forge → `esp_https_ota` | device: deteksi versi baru → tarik → update |
| **5. Secure Boot v2** | sign di CI/`publish-firmware.sh`; verifikasi bootloader; doc enroll kunci | device (sekali kabel): hanya image bertanda-tangan boleh boot |
| **6. Encrypted settings** | `EncryptedConfig` (AES-GCM) untuk value `secret` | host+device: wifi psk tersimpan terenkripsi, terbaca balik |

Fase 0,1,3 bisa diverifikasi sebagian build-only. Fase 2,5 wajib device. Fase 6 host-testable.

---

## 4. Acceptance criteria
- [ ] Dual-slot A/B aktif; update tidak merusak slot aktif (atomic).
- [ ] `IOtaUpdater` mengisi dari **PLP Channel::Ota** dan **WiFi pull** (Forge).
- [ ] Image baru gagal self-test → **rollback otomatis** ke image lama.
- [ ] Secure Boot v2: image tanpa tanda tangan valid **ditolak** boot.
- [ ] Forge: registry menyajikan versi+signature; `/flash` tab OTA push berhasil.
- [ ] Settings `secret` terenkripsi AES-GCM; round-trip benar.
- [ ] Progress + status muncul di UI device (event-driven).

---

## 5. Risiko & catatan
| Item | Catatan |
|---|---|
| Brick | Justru INI alasan plan: secure-boot + rollback bikin OTA aman. Slot lama selalu jadi jaring pengaman. |
| Partisi vs storage | Dua slot app 5MB memangkas storage Plan 38 (11→~6MB). **Koordinasi layout wajib.** |
| Kunci | Secure Boot v2 = e-fuse **sekali tulis** — hati-hati saat enroll (dokumentasikan, jangan otomatis). |
| First flash | Penanaman slot + kunci tetap butuh **sekali kabel**; sesudah itu full OTA. |
| Flash encryption | Opsional, di luar v1 (kompleksitas e-fuse + debug). |
| WASM/host | OTA firmware = konsep device-only; di sim cukup stub/no-op + UI mock. |

## 6. Non-goals (v1)
- Bukan delta/patch update (kirim full image dulu; delta = nanti).
- Bukan multi-image campaign/fleet management (satu device dulu).
- Flash encryption penuh ditunda.
