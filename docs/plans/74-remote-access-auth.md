# 74 — Remote Access: Channel Tiers + Settings→Remote + Session Auth

> Tambahkan **lapisan otorisasi** ke RemoteService/PLP sebelum protokol keluar ke jaringan.
> Tanpa ini, channel CLI/File/OTA = remote-code-execution tanpa kunci. Desain inti: **tier
> channel** (observasi vs privileged) + **password sesi** + **BLE bonding sebagai cache
> otorisasi**. Konsep konsisten lintas transport (USB/BLE/—nanti—TCP).

- Status: 🔴 Not started
- Depends on: 35 (PLP/RemoteService), 73 (WiFi/BLE jalan), 40 (ProfileService — sha256+salt)
- Blocks: **75 (TCP transport — TIDAK boleh online tanpa auth)**, Forge CLI (login)

---

## 0. Konteks & ancaman

`RemoteService` me-route channel PLP: Input, System, Log, Event, **Cli, File, Ota, Ext**.
Hasil grep auth/token di `link/` + `remote_service` = **kosong** — tak ada gating sama
sekali. Sekarang aman karena transport hanya kabel/BLE jarak-dekat; begitu plan 75 menambah
TCP, **siapa pun di jaringan bisa menjalankan CLI, baca/tulis file, push firmware**. Auth
adalah **prasyarat**, bukan fitur opsional.

---

## 1. Model otorisasi (kunci di TIER, bukan transport)

### 1.1 Dua tier channel

| Tier | Channel | Auth |
|---|---|---|
| **Observation** | `Control`, `GetInfo`, `Screen` (mirror), `Log`, `Event` | ❌ read-only, tak bisa merusak |
| **Privileged** | `Cli`, `File`, `Ota`, `Ext` (inject input / install app), `System` (restart/shutdown) | ✅ butuh sesi terotorisasi |

Aturan tunggal, sama semua transport: **"channel privileged ditolak sampai sesi
terotorisasi"**. `RemoteService` mengecek `session.authorized` sebelum me-route frame
privileged; kalau belum, balas `Control: AuthRequired`.

### 1.2 Password default null = aman secara default

- **Password belum diset (null)** → Remote tetap nyala, **hanya tier observation** terbuka.
  Demo out-of-box jalan (mirror layar ke Forge) tapi **tak bisa di-pwn**. Privileged ditolak
  dengan pesan "Set a remote password on the device first".
- **Password diset** → tier privileged terbuka untuk sesi yang lolos auth.

> Ini koreksi dari ide awal "null = bebas": null ≠ akses penuh, null = **privileged terkunci**.
> Lebih aman dan tetap ramah demo.

### 1.3 Auth handshake (PLP `Control` channel)

Challenge–response, password **tak pernah dikirim plaintext**:

```
device → host : AuthChallenge { nonce (16B random), protoVer }
host   → device: AuthResponse { hmac = HMAC-SHA256(password, nonce || transportId) }
device         : hitung ulang, banding constant-time
device → host : AuthOk { sessionToken (32B) }   |   AuthFail { reason }
```

- Reuse `crypto/sha256.h` (sudah ada; ProfileService memakainya). HMAC tipis di atas SHA-256.
- Rate-limit: max 5 percobaan / 30s per transport; lalu lockout sementara (anti brute-force).
- `sessionToken` di-cache host (Forge/CLI) → reconnect cepat tanpa ketik ulang (lihat §2.2).

### 1.4 BLE = bonding meng-cache otorisasi (jawaban "auth BLE yang bagus")

Pisahkan **dua lapisan**, jangan campur:

1. **Link layer** (sudah ada): BLE LE Secure Connections + numeric comparison + bonding →
   enkripsi radio + anti-MITM. Native BLE.
2. **Session layer** (plan ini): otorisasi tier privileged = password.

Penyatuan elegan — analogi `ssh known_hosts + key`:
- Koneksi BLE pertama: pairing numeric → bonded, **lalu** lolos AuthResponse sekali → device
  tandai **bond itu "authorized"** (simpan flag/token di NVS keyed by bond identity).
- Koneksi berikutnya dari bond authorized: link sudah terenkripsi + bond dipercaya → device
  auto-authorize sesi, **tak perlu password lagi**.
- USB/TCP (tanpa bonding): password tiap sesi, atau pakai `sessionToken` tersimpan.

Hasil: satu konsep ("authorized session"); BLE cuma dapat caching gratis dari bonding.

---

## 2. Settings → Remote (menu baru)

`RemoteSettingsScreen` (ComponentScreen, gaya plan 60), digate caps remote
(`caps::RemoteUsb`/`RemoteBle`/—nanti—`RemoteNet`):

```
REMOTE                                [TitleBar]
 ── [Toggle] Remote Enabled            (default: ON)
 ── Password           ▸ Set / Change / Clear
 ── Authorized devices ──
     Forge (USB token) ▸ revoke
     iPhone (BLE bond) ▸ revoke
 ── Status: 1 active session (USB)
```

| Item | Fungsi |
|---|---|
| `Enabled` | Master switch RemoteService (semua transport). Default **ON**. Off = device tak bisa diremote sama sekali. |
| `Password` | Set/clear. Disimpan sebagai **hash+salt** di NVS (reuse pola ProfileService). Null = privileged terkunci. |
| `Authorized devices` | Daftar bond BLE authorized + session token USB/TCP aktif; tiap baris bisa **revoke**. |
| `Status` | Jumlah sesi aktif + transport-nya. |

### 2.1 Persistensi (NVS namespace `remote`)
- `enabled` (bool, default 1), `pwhash`/`pwsalt` (string), `tok_<id>` (token tersimpan),
  `bond_auth_<addr>` (flag BLE authorized). Semua key ≤15 char.

### 2.2 Session token (untuk Forge CLI `auth login`)
- Setelah AuthOk, host simpan `sessionToken`. Reconnect mengirim token (bukan password) →
  device validasi terhadap daftar token. Revoke di Settings = hapus token → sesi/loginnya mati.
- Token punya TTL opsional (mis. 30 hari) + bisa dicabut. Ini pola `doctl auth` / API key.

---

## 3. Perubahan PLP / RemoteService

- `link/plp_codec`: tambah Control opcodes `AuthChallenge/AuthResponse/AuthOk/AuthFail/
  AuthRequired`. (Wire format minor-versioned; bump proto minor, jaga backward-compat handshake.)
- `LinkService`: state sesi `Unauthenticated → Authenticating → Authorized`; gate frame
  privileged saat belum `Authorized`.
- `RemoteService`: cek tier sebelum route; tolak privileged dgn `AuthRequired`; kelola
  token/bond-auth; subscribe toggle `Enabled`.
- `RemoteAuthStore` (core): hash/verify password (sha256+salt), generate/validasi/revoke token,
  set/get bond-auth flag — **di core** (portable), persistence via `IConfigStore`.

## 4. Forge / host (cukup stub di plan ini; implementasi penuh di Forge CLI/web)

- Forge web `RemoteSession`: tangani `AuthRequired/AuthChallenge` → prompt password → kirim
  `AuthResponse` → simpan token. (Logika ini nanti pindah ke `@palanu/link` shared — lihat
  roadmap Forge CLI.)
- Tampilkan di UI Forge: "🔒 Authentication required" sebelum buka panel CLI/Files/OTA.

## 5. Tasks

- [ ] `RemoteAuthStore` (core) + unit test (hash/verify/token/lockout) di `firmware/tests/`.
- [ ] PLP Control opcodes auth + codec (C++ **dan** `packages/forge/src/lib/klp/codec.ts`).
- [ ] `LinkService`/`RemoteService`: tier gating + state sesi + AuthRequired.
- [ ] BLE bond-auth caching (NVS) + auto-authorize bonded.
- [ ] `RemoteSettingsScreen` + wiring di `settings_screen` (gate caps remote).
- [ ] Forge web: handle handshake + simpan token + UI "auth required".
- [ ] Build hijau 3 target + `bun run test`.

## 6. Acceptance criteria

- [ ] Password null → mirror/log/event jalan; CLI/File/OTA **ditolak** dgn `AuthRequired`.
- [ ] Password diset → AuthResponse benar membuka privileged; salah → `AuthFail` + rate-limit.
- [ ] BLE: pairing+auth sekali → reconnect berikutnya **tanpa** password (bond authorized).
- [ ] USB/TCP: token tersimpan → reconnect tanpa re-prompt; **revoke** mematikan akses.
- [ ] `Remote Enabled = off` → semua transport menolak sesi.
- [ ] Password tak pernah ke log/wire dalam plaintext; banding token/hmac constant-time.
- [ ] Tak ada `#include` platform di `core/**` (RemoteAuthStore portable).
