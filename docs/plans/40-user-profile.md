# 40 — User Profile (Owner Info) + Verify API

> **Identitas pemilik device** yang sederhana: **nama user**, **nama device**, dan
> **password (opsional)** — disimpan persisten di NVS, plus **API verify** supaya
> CLI dan custom app (`.kapp`) bisa membaca identitas dan **memverifikasi** apakah
> sebuah input cocok dengan password / apakah password sudah di-set.
>
> ⚠️ **Bukan multi-user, bukan sistem auth.** Ini **hanya menyiapkan fitur + API**.
> Untuk sekarang **tidak ada enforcement sama sekali** — tidak ada lock screen yang
> mengunci, tidak ada gating remote/CLI. Profile cuma *data + fungsi verify* yang
> bisa dipakai pihak lain (mis. custom app yang ingin minta password sendiri).
>
> Asal-usul: diskusi 2026-06-11. Awalnya dibahas sebagai "user system" (multi-user /
> root / permission), tapi disepakati itu overkill & salah-fit untuk device genggam
> satu-orang (AkiraOS & Flipper Zero pun tidak punya multi-user). Yang dibutuhkan
> cukup **satu OwnerProfile**.

- Status: ✅ **DONE** (2026-06-11). Fase 1–4 selesai; Fase 5 (Forge UI) opsional.
- Milestone: M11 (Device Identity).
- Depends on: **24 (Config Store / NVS)**, **05 (Service Container)**, **37 (Embedded
  JS apps — untuk expose API ke `.kapp`)**.
- Enables (kelak, **di luar scope plan ini**): lock screen ber-PIN, auth transport
  remote (BLE/USB/IP), "guest mode".

---

## 0. Keputusan kunci & alasan

### 0.1 Satu `OwnerProfile`, bukan akun multi-user
Tidak ada `login`, `uid/gid`, `su`, atau permission rwx. Device dipegang satu orang;
"user" Unix (mengisolasi banyak manusia berbagi mesin) tidak relevan. Kita simpan
**satu** record identitas. "Root" = konteks system (shell/CLI/core) yang tak dibatasi
— bukan akun yang di-`login`.

### 0.2 Password = **hash bersalt**, bukan plaintext — dan sifatnya *deterrent*
- Disimpan sebagai `salt` + `SHA-256(salt || plain)`. **Tidak pernah** simpan
  plaintext.
- **Jujur soal kekuatan**: flash device bisa di-re-flash / dibaca lewat USB-JTAG,
  jadi password ini **deterrent** (menahan orang iseng), **bukan** proteksi terhadap
  penyerang yang memegang device + komputer. Proteksi sebenarnya butuh flash
  encryption + secure boot (Plan 39, berat, terpisah).
- **Portabilitas**: ProfileService ada di **core** (harus jalan di host/WASM/esp32),
  jadi hashing **tidak** boleh bergantung mbedtls (yang hanya ada di IDF). Vendor
  **SHA-256 kecil (1 file, public domain)** di `core/src/crypto/` agar identik di
  semua platform & teruji di host.

### 0.3 **Tanpa enforcement** untuk sekarang
Plan ini **tidak** menyentuh lock screen dan **tidak** menambah gerbang auth di mana
pun. Tidak ada yang terkunci. Yang disiapkan hanya: simpan profile + fungsi
`hasPassword()` / `verifyPassword()`. Siapa yang memakainya (lock screen, app, remote)
adalah pekerjaan terpisah nanti. Ini sengaja, sesuai permintaan.

### 0.4 Default ditanam saat flash pertama (NVS kosong)
Boot pertama (key belum ada di NVS) → seed:
| Field | Default |
|---|---|
| `user`   (nama user)   | `"Palaner"` |
| `device` (nama device) | `"My Palanu"` |
| password               | **undefined** (tidak di-set) |

Boot berikutnya **tidak menimpa** — nilai yang sudah diubah user tetap.

---

## 1. Goal (acceptance tingkat-tinggi)

1. **`ProfileService`** (core) menyimpan `userName`, `deviceName`, `passwordSalt`,
   `passwordHash` di NVS namespace `"profile"` lewat `IConfigStore`.
2. **Default** ter-seed saat NVS kosong (0.4). Persisten lintas reboot.
3. **API** (C++):
   - `getUserName()` / `setUserName(name)`
   - `getDeviceName()` / `setDeviceName(name)`
   - `setPassword(plain)` / `clearPassword()`
   - `hasPassword()` → bool
   - `verifyPassword(input)` → bool (false bila password tak di-set)
4. **CLI**: `whoami`, `profile` (tampil), `profile set user|device <nilai>`,
   `profile passwd <baru>` / `profile passwd --clear`, `profile verify <input>`.
5. **Custom-app (JS) API** read+verify (lihat 2.4): `palanu.profile.userName()`,
   `deviceName()`, `hasPassword()`, `verifyPassword(input)`. **App tidak bisa men-set**
   profile/password (hindari app jahat mengubah identitas) — set hanya via CLI/Settings.
6. **Nama device dipakai**: saat start, set nama advertised BLE/USB dari
   `deviceName()` (`IBluetoothController::setDeviceName`), dan tampilkan di Forge.
7. Teruji **host + WASM** (unit test ProfileService + hash), build esp32 OK.

**Non-goal (eksplisit di luar scope):** lock screen mengunci, gating auth
remote/CLI, multi-user/akun, flash encryption.

---

## 2. Arsitektur

### 2.1 Service (`firmware/core/include/palanu/services/profile_service.h`)
```cpp
class ProfileService : public IService {
public:
    void init(IConfigStore& cfg);   // load + seed defaults bila kosong

    const std::string& userName()   const { return user_; }
    const std::string& deviceName() const { return device_; }
    void setUserName(const std::string& n);     // persist ke NVS
    void setDeviceName(const std::string& n);   // persist + (caller) re-adv BLE/USB

    bool hasPassword() const { return !hash_.empty(); }
    void setPassword(const std::string& plain); // generate salt + simpan hash
    void clearPassword();
    bool verifyPassword(const std::string& input) const;  // false bila tak di-set

private:
    IConfigStore* cfg_ = nullptr;
    std::string user_, device_, salt_, hash_;   // cache; sumber kebenaran = NVS
};
```
- NVS namespace `"profile"`, key: `user`, `device`, `pwsalt`, `pwhash` (hex string).
- `verifyPassword`: `hasPassword() && hexSha256(salt_ + input) == hash_`
  (bandingkan konstan-waktu sederhana — bukan early-return per-byte).

### 2.2 Hash util (`firmware/core/src/crypto/sha256.{h,cpp}`)
SHA-256 portabel vendored (public domain). Helper:
`std::string hexSha256(const std::string&)`, `std::string randomSalt(n)` (salt dari
sumber acak platform; di host/WASM boleh PRNG sederhana — salt bukan rahasia, cuma
anti-rainbow-table). Teruji dengan test vector standar di host.

### 2.3 Registrasi platform
- esp32 & wasm `registerDrivers`/`postRegister`: `profile_.init(rt.config())`,
  `rt.container().registerService(&profile_)`, `registerAs<ProfileService>`.
- Setelah init: `if (auto* bt = rt.container().resolve<IBluetoothController>())
  bt->setDeviceName(profile_.deviceName())` — nama device tampil saat scan BLE.
- `rt.capabilities().add("profile")`.

### 2.4 Expose ke custom app (`firmware/core/src/js/js_api.cpp`)
Tambah objek `palanu.profile` (read + verify saja):
```js
palanu.profile.userName()            // -> "Palaner"
palanu.profile.deviceName()          // -> "My Palanu"
palanu.profile.hasPassword()         // -> bool
palanu.profile.verifyPassword(input) // -> bool  (use-case utama)
```
Implementasi `api_profile_*` resolve `ProfileService` via `e->host()->container()`.
**Sengaja tanpa setter** — app tak boleh mengubah identitas/owner. Contoh pakai:
app kustom yang ingin "kunci" dirinya sendiri tinggal panggil
`palanu.profile.hasPassword()` lalu minta input dan `verifyPassword(input)`.

### 2.5 CLI (`firmware/core/src/services/cli_service.cpp`)
Tambah command (resolve `ProfileService` dari container):
```
whoami                       -> userName (+ deviceName)
profile                      -> tampil user/device/password-set?
profile set user <nama>
profile set device <nama>    -> + re-adv BLE name
profile passwd <baru>
profile passwd --clear
profile verify <input>       -> "ok" / "no" (uji manual API verify)
```

---

## 3. Fase pengerjaan

- [x] **Fase 1 — Core service + hash.** `firmware/core/include/palanu/crypto/sha256.h`,
      `firmware/core/src/crypto/sha256.cpp` (XorShift32 PRNG salt + FIPS 180-4 SHA-256).
      `ProfileService` (load/seed/get/set/verify + constant-time compare). Core CMake updated.
- [x] **Fase 2 — Registrasi platform + nama device.** Wired di esp32 & wasm; BLE adv name
      diset dari `profile_.deviceName()` setelah remote init; capability `"profile"` ditambah.
- [x] **Fase 3 — CLI.** `whoami` (user + device name) dan `profile` (set user|device,
      passwd, verify) di `registerCoreCliCommands`.
- [x] **Fase 4 — Custom-app API.** `palanu.profile.userName/deviceName/hasPassword/verifyPassword`
      di `js_api.cpp`; gated dengan capability check; setters sengaja tidak ada.
- [ ] **Fase 5 — Forge (opsional).** Tampilkan device/user name di header sesi;
      panel Settings "Profile" untuk lihat/ubah (lewat CLI atau channel System).
      *Boleh ditunda; firmware sudah cukup tanpa ini.*

**Build/uji**: host + WASM tiap fase; esp32 build-only di akhir.

---

## 4. Yang sengaja TIDAK dikerjakan (catatan untuk masa depan)

- **Lock screen mengunci pakai password** — UI + state ada (virtual keyboard,
  text input), tinggal panggil `verifyPassword` di unlock. **Plan terpisah.**
- **Auth remote (BLE/USB/IP)** — gating koneksi pakai profile/credential. Terpisah,
  relevan saat transport IP masuk.
- **Multi-profil / ganti user** — menghidupkan kembali kompleksitas multi-user;
  hindari kecuali ada kebutuhan nyata. "Guest mode" cukup satu flag bila perlu.
- **Proteksi kuat** — flash encryption + secure boot = Plan 39.
