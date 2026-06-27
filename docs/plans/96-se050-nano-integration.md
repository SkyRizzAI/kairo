# 96 — SE050 real integration: NXP nano-package + mode-B seed sealing

> **Riset + rencana.** Plan 94 membangun seluruh wallet (Fase 1–7) dengan custody **software**
> (mode C, PIN-encrypted) dan menyiapkan jalur **mode B** (seed di-*wrap* oleh secure element)
> lewat `WalletVault` yang sudah SE-aware + auto-select berbasis capability. Yang **belum** ada:
> driver SE050 yang benar-benar bicara ke chip. Percobaan hand-rolled T=1'oI2C (`se050_driver.cpp`,
> `kEnableBringup`) **gagal**: chip merespons tapi framing salah (CRC mismatch) + memicu boot-loop
> (heap). **Kesimpulan riset: jangan hand-roll protokolnya** — pakai library NXP **nano-package**
> yang sudah menangani T=1'oI2C + APDU dengan benar. Plan ini mengintegrasikan nano-package,
> menulis *platform glue* ESP32, dan mengaktifkan **mode B (AES seed-seal)** sehingga wallet di
> skyrizz-e32 memakai SE050 asli — sambil tetap fail-closed ke software bila SE tak tersedia.

- Status: ✅ **DONE — SE050 mode-B aktif & tervalidasi di hardware** (skyrizz-e32). Boot log:
  `secureStore=ENABLED (wallet -> mode B)` + `[WalletSystem] mode=B (secure-element)`. Board normal
  (touch/mic/tombol jalan, boot ~3s). Kunci: **Opsi A2** — bus handle arduino Wire dipakai bareng +
  SE050 device sendiri dgn `scl_wait_us` (tahan clock-stretch). `appletVer=0301016fff010b`.
- Prasyarat: Plan 94 (wallet engine + WalletVault SE-aware + capability auto-select) — **selesai**.
- Board: SkyRizz E32, **`SE050C2HQ1`** @ I²C `0x48`.

---

## 0. Hardware (terkonfirmasi dari `refs/skyrizz-e32` + pin map)

| Aspek | Detail | Sumber |
|---|---|---|
| Part | **`SE050C2HQ1`** (varian **C2**) | `refs/skyrizz-e32/README.md` |
| Bus | **I²C-only**: SDA `GPIO47`, SCL `GPIO48`, addr **`0x48`** | pin map `U18` |
| Kontak ISO7816 | **n.c.** semua (mode kontak tak dipakai) | pin map `U18` |
| Reset | `ISO7816RST_N` **active-LOW** ← XL9535 **`P03`**. `Xl9535::setSeReset()` polaritas **benar** ✓ | pin map + `xl9535.cpp:122` |
| Enable | `ENA` di-pull-up ke 3V3 via `R43` → selalu aktif (tak perlu kontrol) | pin map `U18` |
| IRQ | Tak ada IRQ SE050 khusus (GPIO43 = INT XL9535) → murni I²C polling | pin map |

**Kapabilitas silikon C2:** AES + ECC NIST (P-256/P-384) + RSA + **secp256k1** (di chip), **TANPA
Ed25519** (itu varian E). Untuk wallet: yang kita butuh = **AES** (seed-seal mode B). secp256k1
in-chip signing **tidak** dipakai di tahap ini (signing tetap software/HdWallet — sesuai ADR 0014).

---

## 1. Keputusan

| # | Keputusan | Alasan |
|---|---|---|
| K1 | **Pakai NXP `nano-package`** (bukan full `simw-top`, bukan hand-roll) | ~1KB RAM, Apache-2.0, port platform kecil (6 fungsi), library yang handle T=1'oI2C+APDU dengan benar. Hand-roll terbukti gagal (CRC/boot-loop). |
| K2 | **Plain session** (`PLUGANDTRUST_SE05X_AUTH=None`) dulu | Tak perlu provisioning SCP03 → bisa langsung. Cukup untuk AES seed-seal di config default. SCP03 = hardening pre-ship (Fase D). |
| K3 | **Mode B = AES seed-seal saja**, signing tetap software | nano-package punya AES-CBC (cukup untuk wrap seed). secp256k1 in-chip tidak di nano-package → signing tetap HdWallet. Sesuai ADR 0014 (SE jaga seed at-rest; signing software). |
| K4 | **Host crypto = mbedTLS** (di ESP-IDF), fallback **tinyCrypt** (dibundle nano) | ESP-IDF sudah punya mbedTLS. Risiko: nano minta mbedtls **2.x**, IDF 5.5 = **3.x** → kalau bentrok, pakai tinyCrypt. |
| K5 | **Tetap fail-closed** | Driver baru lolos `selfTestSeal()` (wrap→unwrap roundtrip saat boot) → baru `hasFeature(SecureStore)=true`. Gagal → wallet software. Tak pernah brick. |
| K6 | **StetelThings component DITOLAK** | Full middleware, **IDF4-only** (tak compile di IDF5.5). nano-package platform-agnostic → kita tulis port IDF5 sendiri. |

---

## 2. Arsitektur integrasi

```
WalletVault (SE-aware, sudah ada)  ── wrap/unwrap ──▶  ISecureElement
                                                          │
                                          ┌───────────────┴───────────────┐
                                   Se050NanoDriver (BARU)          SimSecureElement / none
                                   - implement ISecureElement       (sim → software)
                                   - wrap()  = SE05x AES-CBC enc
                                   - unwrap()= SE05x AES-CBC dec
                                   - hasFeature(SecureStore)=true setelah selfTest
                                          │ panggil
                                   nano-package API (sss / se05x)
                                   - Se05x_API_CreateSession (None)
                                   - Se05x_API_WriteSymmKey (AES-256, sekali)
                                   - Se05x_API_CipherOneShot (AES-CBC)
                                          │ pakai
                                   T=1'oI2C + APDU  (di-handle library)
                                          │ via platform glue (6 fungsi, BARU)
                                   se05x_platform_i2c_*  → Wire (GPIO47/48 @0x48)
                                   se05x_platform_reset  → Xl9535::setSeReset()
                                   se05x_platform_delay_ms → delay()
```

`Se050Driver` hand-rolled lama → **diganti** `Se050NanoDriver` (atau Se050Driver di-rewrite memakai
nano). `WalletVault` + `bootWalletSystem` + capability auto-select **tidak berubah** (sudah siap).

---

## 3. Fase & checklist

### Fase A — Integrasi library + platform glue ✅ SELESAI
- [x] Vendor `nano-package` (Apache-2.0) → IDF component `firmware/targets/skyrizz-e32/components/se05x/`
      (`vendor/apdu` + `vendor/t1oi2c` + LICENSE).
- [x] `CMakeLists.txt` komponen: compile sumber nano auth=**None** (no SCP03), define `T1oI2C`/`T1oI2C_UM11225`.
      Board `skyrizz-e32` REQUIRES `se05x`.
- [x] **Platform glue ESP32** di `port/`: `sm_i2c.cpp` (`axI2CInit/Term/Write/Read` via **Arduino Wire**,
      8-bit addr 0x90 → 7-bit 0x48), `sm_timer.c` (`sm_sleep`/`sm_usleep` via FreeRTOS), `sm_port.h`
      (logging→printf, malloc, mutex no-op). Reset HW (XL9535 P03) dilakukan driver, bukan port.
- [x] ~~mbedTLS v2/v3~~ **N/A** — plain session AES tak butuh host crypto (AES di dalam chip). Risiko hilang.
- [x] **Build: `libse05x.a` + firmware link OK (exit 0)** — protokol T=1'oI2C (81KB) kini library, bukan hand-roll.

### ✅ Opsi A2 (BERHASIL) — i2c_master + scl_wait_us via shared bus handle
- [x] `sm_i2c.cpp` ambil bus handle Wire (`i2cBusHandle(0)`) + `i2c_master_bus_add_device(0x48)`
      dgn `scl_wait_us=1000000` → `i2c_master_transmit/receive`. **Driver sama dgn Wire → nol konflik.**
- [x] Board driver TIDAK diubah (tetap Wire) → semua device I²C jalan. SE050 device terpisah dgn stretch tolerance.
- [x] Hapus presence-probe Wire (mencegah duplikat 0x48 yg blokir add-device).
- [x] **HW: session=1, sealKey=1, selfTest=1, secureStore=ENABLED, mode=B.** ✅

### Fase B — Bukti komunikasi (de-risk) — ✅ SELESAI (lewat Opsi A2)
- [x] `Se050Driver::openSession()` — HW reset + `Se05x_API_SessionOpen` (plain). **Compile OK.**
- [x] **Flash + jalan di HW**: library hidup → cetak `Plug and Trust nano package - version: 1.6.0`,
      masuk `Se05x_API_SessionOpen → phNxpEse_init → phNxpEseProto7816_Open → phNxpEse_waitForWTX` →
      baca I²C. **Protokol nano BENAR** (vs hand-roll). 
- [ ] **BLOCKER ditemukan**: crash `assert xQueueGenericSend (queue.c:937)` di **`i2c_master_receive`
      (driver IDF i2c-ng)** dipanggil dari `axI2CRead` (Wire) saat SE050 **NACK / clock-stretch**
      selama WTX-polling. Driver arduino/IDF i2c-ng tak tahan semantik NACK-poll T1oI2C SE050 → boot-loop.
      Sementara: `kEnableBringup=false` (wallet software, aman).
- **ROOT CAUSE (riset, terkonfirmasi):** SE050 **clock-stretch**. Arduino-esp32 3.x `Wire` pakai
  **driver IDF `i2c_master` baru** yang **known-broken untuk clock-stretching** — issue resmi Espressif
  [#14401](https://github.com/espressif/esp-idf/issues/14401),
  [#14464](https://github.com/espressif/esp-idf/issues/14464) ("legacy works, new does not"),
  [#11947](https://github.com/espressif/esp-idf/issues/11947),
  [#14667](https://github.com/espressif/esp-idf/issues/14667) (S3 stuck 20ms). `scl_wait_us` pun masih bermasalah.
- **SOLUSI terbukti:** **driver IDF legacy** (`driver/i2c.h`) + `i2c_master_cmd_begin` **timeout 1000ms**
  + ACK/NACK byte benar — dipakai [StetelThings SE050-ESP32](https://github.com/StetelThings/ESP32_SE050_Middleware_Component)
  (referensi yang jalan) & komunitas NXP.
- **BLOCKER arsitektur:** board pakai `Wire` (driver baru) untuk XL9535/touch/mic/sensor di **bus sama**
  (GPIO47/48, satu port). Legacy + baru **tak bisa coexist satu port** (IDF tolak). → harus PILIH:
  - **Opsi A (DICOBA → GAGAL):** migrasi semua I²C board ke driver legacy (`PalWire` shim +
    sm_i2c legacy). Compile OK, **tapi di HW semua device gagal ACK** (SE050/FT6336/ES7243E no-ACK,
    XL9535 start 5 detik, tombol mati). **arduino-esp32 3.x dan driver legacy IDF tak kompatibel di
    port sama** — arduino sudah klaim peripheral i2c (new driver); legacy-ku tak nyambung benar.
    → **di-revert**, board balik ke arduino Wire (jalan), SE gated software. `pal_wire.{h,cpp}` +
    `sm_i2c.cpp` legacy ditinggal (tak dipakai) sbg referensi.
  - **Opsi A2 (next):** coba driver **i2c_master baru** dgn config clock-stretch eksplisit
    (`scl_wait_us` besar, `xfer_timeout_ms`) + akses bus arduino via handle-nya — riset bilang masih
    buggy, tapi belum dicoba dgn config penuh. Atau **drop arduino-esp32 untuk I²C** (semua device
    pakai i2c_master langsung) — besar.
  - **Opsi B:** tetap Wire untuk board, SE050 via legacy dgn uninstall/reinstall Wire saat transaksi SE — rapuh.
  - **Opsi C:** tunggu Espressif fix driver baru (di luar kendali).
- [ ] Putuskan opsi → implement axI2CRead/Write legacy → validasi `session open` + AES (AN12436).

### Fase C — Mode B di wallet — 🟡 kode selesai, tunggu validasi HW
- [x] `Se050Driver` (rewrite, pakai nano-package): `wrap()`/`unwrap()` via `Se05x_API_CipherOneShot`
      AES-CBC; `ensureAesKey()` buat AES-256 object persisten (key dari ESP32 TRNG, ditulis ke chip
      lalu non-extractable); `randomBytes` = ESP32 TRNG (nano tak expose GetRandom). **Compile OK.**
- [x] `hasFeature(SecureStore)` gated `selfTestSeal()` (wrap→unwrap roundtrip) — fail-closed.
- [x] Hand-rolled T=1'oI2C **dibuang** (diganti nano). `kEnableBringup=true` (self-test tetap gate).
- [x] **Build firmware lengkap (exit 0)** — `Se050Driver` + nano-package + wallet semua link.
- [ ] **Flash + verifikasi end-to-end**: log `selfTest=1 secureStore=ENABLED` + `[WalletSystem] mode=B` +
      modal sign "Secure Element" + create/unlock butuh **PIN + chip** (cabut chip → unlock gagal).

### Fase D — Hardening (pre-ship, deferred)
- [ ] Naikkan ke **SCP03** (provisioning kunci platform) — `ECKey`/`PlatfSCP03`.
- [ ] Secure Boot v2 + Flash Encryption (lihat ADR 0014 threat model).
- [ ] (Opsional) in-chip **secp256k1** signing untuk EVM/BTC bila pindah ke full middleware.

---

## 4. Risiko

| Risiko | Mitigasi |
|---|---|
| **mbedTLS 2.x (nano) vs 3.x (IDF 5.5)** | Pakai **tinyCrypt** yang dibundle nano; atau shim API. Cek di Fase A. |
| Config chip blok AES di plain session | Validasi `se05x_crypto`/AN12436 di Fase B sebelum lanjut. |
| Build size / heap | nano ~1KB RAM; mbedTLS sudah ada. Pantau. |
| Butuh iterasi hardware (flash+log) | Kali ini **library** yang handle protokol → jauh lebih mungkin sukses dari hand-roll. Fase B = gate de-risk. |
| Regresi boot (seperti hand-roll) | Tetap fail-closed (selfTest gate) + capability-driven → SE off = software, tak brick. |

---

## 5. Definition of Done
- skyrizz-e32: buat wallet → seed di-**seal AES oleh SE050** (mode B); unlock butuh PIN + chip fisik.
- Boot log `mode=B (secure-element)`; modal sign "Secure Element".
- Simulator & board tanpa SE → tetap software (mode C) — tak ada regresi.
- ADR baru bila ada keputusan non-obvious (mis. pilih tinyCrypt vs mbedTLS, atau plain-vs-SCP03).

---

## Sumber
- [NXPPlugNTrust/nano-package](https://github.com/NXPPlugNTrust/nano-package) — Apache-2.0, minimalis
- [NXP/plug-and-trust](https://github.com/NXP/plug-and-trust) — full middleware (referensi)
- [StetelThings/ESP32_SE050_Middleware_Component](https://github.com/StetelThings/ESP32_SE050_Middleware_Component) — pola integrasi (IDF4, ditolak)
- [AN12413 — SE050 APDU Specification](https://www.nxp.com/docs/en/application-note/AN12413.pdf)
- [AN12436 — SE050 configurations](https://www.nxp.com/docs/en/application-note/AN12436.pdf)
- [SE050 Datasheet](https://www.nxp.com/docs/en/data-sheet/SE050-DATASHEET.pdf)
- `refs/skyrizz-e32/README.md`, `refs/dev-board-1-pin_map.md` (§Secure element)
- ADR 0014 (key-mode & threat model), ADR 0015 (wallet arsitektur), Plan 94
