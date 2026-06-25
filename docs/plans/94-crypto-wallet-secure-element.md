# 94 — Crypto Wallet + Secure Element: Wallet API, dua backend, dApp signing

> **Riset + rencana.** User minta fitur **hardware crypto wallet** sekelas Trezor/Ledger di
> SkyRizz E32 (punya NXP **SE050**): private key di-custody secure element, konfirmasi transaksi
> lewat **trusted display + tombol fisik**. Dua API: (1) **Secure Element API** (privileged HAL,
> permission-gated) dan (2) **Wallet API** (abstraksi di atas SE, backend pluggable — pakai SE
> kalau ada, fallback software/NVS kalau tidak), plus **dApp signing flow** (pola MetaMask/Phantom),
> dengan **app "Wallets"** di launcher.
>
> **Temuan riset:** fondasinya **sudah dibangun sebagian** — ADR 0005 sudah memutuskan SE sebagai
> generic capability-gated HAL; `ISecureElement` (`firmware/core/include/nema/hal/secure_element.h`)
> sudah ada; `Se050Driver` sudah **scaffolded** (reset + I²C presence probe @0x48 jalan; crypto ops
> `genKey`/`sign` masih `return false` TODO); `SimSecureElement` (WASM) ada; `caps::Secure` sudah
> dideklarasikan & board sudah `registerAs<ISecureElement>`. PermissionService + modal (Plan 87),
> VirtualKeyboard QWERTY (Plan 23), AppStorage `criticalStorage()` (Plan 83), transport PLP/Forge
> (Plan 35/88), libsodium (Ed25519) + mbedTLS — **semua sudah ada**. Plan ini **bukan dari nol** —
> ini **mengisi TODO crypto SE050, menambah software crypto core, membangun Wallet API + dua backend,
> consent modal sistem, app Wallets, dan dApp bridge.**

- Status: ☐ Planned (riset selesai, fondasi SE HAL + UI + permission sudah ada)
- Milestone: M9+ (Ecosystem / Forge maturation)
- Depends on: **ADR 0005 (SE generic HAL)**, 83 (Storage/AppStorage + NVS), 87/89 (Permission model + UI),
  23 (VirtualKeyboard — restore phrase), 37/58 (embedded JS apps — host binding), 35/88 (PLP remote —
  dApp over transport), 90 (Aether UI)
- Blocks: "cold wallet connect ke Phantom" (Fase 8, next goal)
- Catatan keamanan: **Secure Boot v2 + Flash Encryption sengaja DITUNDA** ke pre-ship (§1).
  Selama dev → **testnet / throwaway key only**.

---

## 0. Prinsip & keputusan yang dikunci

| # | Keputusan | Alasan |
|---|---|---|
| K1 | **Nama API = `wallet`, bukan `web3`** | "web3" buzzword & basi; Bitcoin bukan "web3". dApp-provider boleh disebut "web3 provider" sebagai *fitur di dalam* wallet. |
| K2 | **Bangun di backend software dulu, SE050 nyusul swap-in** | Backend software nol keterbatasan silikon → seluruh wallet jalan tanpa nunggu SE050. Desain pluggable-backend = otomatis menyelesaikan blocker SE. |
| K3 | **Tiga concern dipisah tegak lurus: Custody × Chain × Consent** (§2) | Mencegah ledakan N×M; tiap concern berevolusi independen. Ini tulang punggung arsitektur. |
| K4 | **Key-mode default = "Seed-in-SE (wrapped)"** (§2.5, ADR 0014) | Satu-satunya mode yang kasih **recovery phrase** DAN jalan untuk **semua chain** terlepas dukungan silikon SE050. |
| K5 | **Wallet API TIDAK PERNAH expose private key / seed** | Hanya `getAddress`/`signTransaction`/`signMessage`. Hard rule. |
| K6 | **Sign = consent SISTEM, per-transaksi, tombol fisik, fail-closed** (§3) | WYSIWYS. Modal dimiliki sistem (bukan app), decode == sign, tak pernah di-"remember". |
| K7 | **`se.raw` privileged — tidak pernah ke app pihak ketiga** | Sign arbitrary digest = bypass WYSIWYS. Internal wallet service only. |
| K8 | **Semua consumer (native/JS/web dApp) → satu WalletService → satu consent modal** (§4) | Flow seamless & konsisten; satu jalur keamanan, bukan tiga. |
| K9 | **Tidak ada operasi yang membekukan UI** | Sign/derive jalan di worker thread (Nema TaskRunner); UI thread render modal; thread peminta block. (Konvensi Plan 19.5.) |

---

## 1. Model ancaman — apa yang SE050 lindungi & TIDAK (→ ADR 0014)

> Komunikasikan jujur ke user di UI — **jangan tulis "as secure as Ledger".**

**Dilindungi SE050 (EAL6+, tamper/side-channel resistant):**
- Seed/private key **tidak bisa di-extract** walau device dicuri & dibongkar.
- Malware remote **tidak bisa exfiltrate** key (tak pernah plaintext-persisten di flash/PSRAM).

**TIDAK dilindungi (plafon = integritas firmware ESP32):**
- SE050 = **signing/unwrap oracle buta**: tak ada layar/tombol; ia proses apa pun yang dikirim ESP32.
  Firmware ESP32 compromised → attacker bisa minta unwrap/sign tx jahat; layar ST7789 (digerakkan
  ESP32) bisa menampilkan tx palsu. **"Trusted display" cuma se-trusted Secure Boot-mu.**
- Mitigasi: **Secure Boot v2 + Flash Encryption** (ditunda ke pre-ship), **SCP03 + host-binding**
  ESP32↔SE050, **PIN/user-presence gating** (§6 onboarding), **jauhkan radio dari jalur signing**.

**Posisi realistis:** target **"Trezor Safe-class"** (MCU + SE untuk proteksi key; derivation &
display di MCU; andalkan Secure Boot) — **bukan Ledger-class** (app+display di dalam SE bersertifikat;
tak bisa direplikasi dengan SE050 fixed-function).

**Dev-mode sekarang:** Secure Boot OFF (cepat, anti-brick) → wallet dev setara hot wallet →
**testnet/devnet/throwaway key only**. Promote ke dana asli **hanya** setelah Secure Boot ON.

---

## 2. Arsitektur — tiga concern tegak lurus (→ ADR 0015)

Kunci desain: jangan campur "di mana key disimpan", "gimana chain bekerja", dan "gimana user setuju".
Tiga sumbu independen yang **compose**, bukan **multiply**:

```
              ┌──────────────────────── WalletService (orkestrator) ───────────────────────┐
              │                                                                              │
   CUSTODY ───┤  IWalletBackend   (SE050 wrapped-seed | software-NVS)                        │
   (di mana)  │     → buta chain. "kasih path+curve+digest, kukembalikan signature."         │
              │                                                                              │
   CHAIN ─────┤  IChain           (EVM | Bitcoin | Solana) + NetworkRegistry (data)          │
   (gimana)   │     → buta custody. address, serialisasi, hashing, decode-for-display.       │
              │                                                                              │
   CONSENT ───┤  SignConsent      (system modal: WYSIWYS + tombol fisik, fail-closed)        │
   (setuju?)  │     → buta keduanya. "tampilkan ini, tunggu approve fisik."                  │
              └──────────────────────────────────────────────────────────────────────────────┘
```

Konsekuensi: tambah chain = +1 `IChain` (jalan di semua backend). Tambah network EVM = +1 baris data.
Tambah custody = +1 `IWalletBackend` (jalan di semua chain). Tak ada `chains × backends`.

### 2.1 Sumbu CUSTODY — `IWalletBackend` (chain-blind, level kurva)

```cpp
enum class Curve { Secp256k1, Ed25519 };

struct IWalletBackend {
  virtual BackendKind kind() const = 0;                 // SecureElement | Software  → indikator UI
  virtual bool publicKey(const DerivationPath&, Curve, PubKey& out) = 0;
  // Tandatangani SATU payload. TIDAK PERNAH balikin private key (K5).
  //   secp256k1 (ECDSA): payload = digest 32-byte (prehashed=true), RFC6979 + low-S.
  //   ed25519   (EdDSA): payload = pesan UTUH (prehashed=false) — EdDSA hash internal.
  virtual bool sign(const DerivationPath&, Curve,
                    const uint8_t* payload, size_t n, bool prehashed, Signature& out) = 0;
};
```
Implementasi: `SeBackend` (seed di-wrap SE050) & `NvsBackend` (seed terenkripsi software). Keduanya
chain-agnostic; sama-sama derive BIP32 dari seed lalu sign — beda cuma **custody seed-at-rest**.

### 2.2 Sumbu CHAIN — `IChain` (satu driver per *family*, network = data)

```cpp
struct ChainInfo { const char* family; uint32_t bip44CoinType; Curve curve; };
struct SigningItem { DerivationPath path; Curve curve; Bytes payload; bool prehashed; };

struct IChain {
  virtual ChainInfo info() const = 0;
  virtual std::string  addressFromPubkey(const PubKey&, const NetworkParams&) const = 0;
  virtual DerivationPath pathFor(uint32_t account, const NetworkParams&) const = 0;
  virtual TxPreview    decodeForDisplay(const Bytes& rawTx, const NetworkParams&) const = 0; // WYSIWYS
  // Bisa >1 item: BTC PSBT = satu per-input; EVM = satu (digest keccak); Solana = satu (pesan utuh).
  virtual std::vector<SigningItem> buildSigningPayload(const Bytes& rawTx, const NetworkParams&) const = 0;
  virtual Bytes        encodeSigned(const Bytes& rawTx, const std::vector<Signature>&, const NetworkParams&) const = 0;
  virtual MsgPreview   decodeMessage(const Bytes& msg, MsgKind) const = 0;  // personal_sign/EIP-712/...
};
```
Cuma **3 implementasi**: `EvmChain`, `BitcoinChain`, `SolanaChain`.

**Network = data, bukan driver.** Ethereum/Polygon/Arbitrum/Base/BSC = `EvmChain` yang sama, beda
`chainId`. Registry network berbasis data:
```cpp
struct NetworkParams { const char* id; const char* family; uint64_t chainId; bool testnet; /*rpc hints*/ };
// registry: "eth-mainnet"(evm,1) "polygon"(evm,137) "arbitrum"(evm,42161) "btc-testnet"(bitcoin)
//           "sol-devnet"(solana) ... → tambah L2 baru = +1 baris, nol kode.
```
> MVP: registry = tabel **constexpr embedded** (tepercaya, tanpa injeksi). Network tambahan dari user
> (future) wajib divalidasi sebelum dipakai.

### 2.3 Sumbu CONSENT — lihat §3 (system modal).

### 2.4 Model akun (Wallet → Account)

- **Wallet** = satu BIP39 seed (MVP: 1 wallet; desain siap multi-wallet).
- **Account** = `(walletId, networkId, index, address, label)` — BIP44 `m/44'/coinType'/index'/...`.
  Satu seed → banyak account lintas chain (EVM 0x.., BTC bc1.., SOL base58).
- **Storage:** seed (wrapped/terenkripsi) di `criticalStorage()` (flash-only, no SD); metadata akun
  (label, network aktif, connected apps) di `AppStorage` biasa. **Seed tak pernah keluar dari core.**

### 2.5 Key-mode (default = Seed-in-SE wrapped) — ringkas (detail di ADR 0014)

SE050 fixed-function → **tak bisa BIP32 di dalam chip**. Tiga mode:

| Mode | Recovery phrase | Key di RAM saat sign | Multichain | Indikator |
|---|---|---|---|---|
| A. SE-native in-slot | ❌ | ❌ tak pernah | hanya curve silikon (Ed25519 ✗ di SE050 biasa) | 🔒 SE |
| **B. Seed-in-SE wrapped** ⭐ | ✅ | ⚠️ sebentar → wipe | semua | 🔒 Secure Element |
| C. Software/NVS | ✅ | ⚠️ saat sign | semua | ⚠️ Software key |

**B dipilih** (K4): seed BIP39 terenkripsi di NVS, **kunci pembungkus hidup di SE050** (AES/SecureStore);
saat sign → unwrap di SE → derive BIP32 di RAM (trezor-crypto) → sign → **wipe**. Memberi recovery
phrase + jalan untuk Solana. **Nuansa jujur indikator:** di B signing tetap di software (RAM); SE
melindungi **seed-at-rest**. "Secure Element" = "seed dijaga SE", bukan "sign di dalam SE".

### 2.6 Catatan kripto (jangan keliru — sumber bug klasik wallet)

- **Derivasi beda per kurva:** BIP32 hanya untuk **secp256k1**; **Ed25519 pakai SLIP-0010**
  (hardened-only). Path Solana lazimnya `m/44'/501'/x'/0'` (semua hardened).
- **Payload sign beda per kurva:** ECDSA secp256k1 → tandatangani **digest 32-byte** (keccak256 EVM /
  double-SHA256 BTC), wajib **RFC6979 deterministic + low-S**. EdDSA Ed25519 → tandatangani **pesan
  utuh** (hash internal), jangan pre-hash. Inilah alasan `IWalletBackend::sign` punya flag `prehashed`.
- **BTC = multi-signature:** satu PSBT bisa punya banyak input → `buildSigningPayload` balikin **list**
  `SigningItem`, `encodeSigned` terima **list** `Signature`.

---

## 3. Consent & permission — dua tingkat (→ ADR 0014/0015)

Tiru model MetaMask: **connect sekali (diingat), tiap sign minta approve (tak pernah diingat).**

| | **Permission** (akses) | **Consent** (tandatangan) |
|---|---|---|
| Contoh | `wallet.read` — connect, lihat address | tiap `signTransaction`/`signMessage` |
| UI | PermissionService modal (Plan 87): "Allow [app] lihat wallet?" | **SignConsent modal (WYSIWYS)** |
| Diingat? | ✅ persist per-app/origin | ❌ **tak pernah** — tiap kali tanya |
| Approve via | tombol fisik | tombol fisik **saja** |
| "Always allow"? | boleh (connect) | **DILARANG** (signing) |

Dua permission yang **di-persist** (PermissionService, sesuai manifest `needs`): **`wallet.read`**
(connect + lihat akun) dan **`wallet.sign`** (app **boleh *meminta*** tandatangan). `wallet.sign` cuma
gate *siapa yang boleh minta* — **approval aktual tiap tx tetap consent per-tx yang tak pernah diingat**
(K6). Jadi sign butuh **keduanya**: app punya izin `wallet.sign` **dan** user approve fisik tiap kali.
Ini cocok dengan permintaan awal (manifest declare `wallet.read`/`wallet.sign`/`se.raw`) tanpa melanggar
"signing tak pernah di-remember".

### Consent modal = milik SISTEM, bukan app (jantung keamanan)

WYSIWYS hanya valid kalau modal di-render kode tepercaya & menampilkan decode atas **bytes yang
benar-benar akan ditandatangani**. Tiru pola **PermissionService → GuiService → `ScreenMode::Modal`**
yang sudah ada; konten lebih kaya (`SignConsentScreen`).

Flow:
```
App/dApp ─signTransaction(rawTx)─► WalletService            (thread peminta BLOCK; jalan di TaskRunner)
   chain.decodeForDisplay(rawTx) │  ← SISTEM yang decode, dari bytes asli (decode == sign)
   publish SignRequest ──────────► GuiService → SignConsentScreen (Modal, GLOBAL di atas app apapun)
        ┌──────────────────────────────────┐
        │ 🔒 Secure Element   [eth-mainnet] │  ← indikator backend + network
        │ from: app.uniswap.org             │  ← SIAPA yang minta (anti-phishing)
        │ Send 0.1 ETH                      │  ← decoded (clear-sign)
        │ To   0x1a2b…f9                    │
        │ Fee  ~0.0003 ETH                  │
        │ [▸ Raw data]                      │  ← expand hex/calldata mentah
        │ [Reject]              [Approve]    │  ← label dari hintFor(Action)
        └──────────────────────────────────┘
   user pencet TOMBOL FISIK → keputusan balik ke WalletService
   approved → backend.sign(tiap SigningItem) → chain.encodeSigned → return ke app
   reject/timeout/Back/app-mati → FAIL-CLOSED, app tidak dapat sig
```

Properti wajib: **(1)** system-owned (app tak bisa spoof/baca/bypass), **(2)** decode == sign (preview
dari bytes yang sama persis yang dikirim ke backend), **(3)** approve hanya fisik (tak ada jalur RPC),
**(4)** fail-closed (Back/timeout/onStop → auto-reject, seperti `PermissionScreen::onStop()`),
**(5)** serialized (1 request aktif; sisanya antri), **(6)** input captured (app belakang tak bisa
curi tombol), **(7)** tampilkan **origin/app id** (anti-phishing), **(8)** blind-sign warning mencolok
kalau decode gagal.

---

## 4. Bridge — tiga consumer, satu core (seamless)

Tiga permukaan consumer, **satu** WalletService, **satu** consent modal (K8):

```
(1) Native C++ app  ─► resolve<WalletService>() langsung
(2) On-device JS/WASM app ─► host binding nema.wallet.* (pola perm.request)  ┐
(3) Web dApp (browser HP/PC) ─► provider shim (TS) ─PLP/Forge transport────► ┼─► WalletRequestRouter ─► WalletService ─► consent modal
     EIP-1193/6963 (EVM) · Wallet Standard (SVM) · signPsbt (BTC)            ┘        │
                                                                    enforce: connect→wallet.read (permission)
                                                                             sign  →SignConsent (consent)
```

**WalletRequest envelope (seragam untuk ketiga permukaan):**
```
connect            → butuh wallet.read (permission, diingat) → balikin accounts (zero secret)
getAccounts        → wallet.read
signTransaction    → consent modal per-tx
signMessage        → consent modal (personal_sign / EIP-712 / SVM message)
disconnect / events(accountsChanged, chainChanged)
```
Broadcast ke network **dilakukan app/dApp sendiri** (device cuma sign). Channel PLP wallet-RPC pakai
reqId-correlation Plan 88 + auth Plan 74. Identitas pemanggil (app id / web origin) **wajib** ikut di
envelope → ditampilkan di consent modal (anti-phishing).

Seamless: request dari Phantom-di-HP via BLE memunculkan **modal yang sama** seperti app on-device.
Satu UX, satu jalur audit keamanan.

---

## 5. Spike SE050 — BLOCKER teknis, paralel (Fase 0)

Semua yang menyentuh SE bergantung pada dukungan silikon nyata. Wajib dibuktikan dulu.

- [ ] Cek **part number** SE050 fisik (SE050 / SE050E / SE051).
- [ ] Spike via NXP **Plug & Trust** (APDU/T=1 over I²C @0x48):
  - [ ] `randomBytes` (TRNG) + `uniqueId` — konfirmasi link APDU hidup.
  - [ ] sign **secp256k1** (klaim BTC/EVM in-slot mode A).
  - [ ] sign **Ed25519** (kemungkinan gagal di SE050 biasa — konfirmasi).
  - [ ] **AES wrap/unwrap** atau **SecureStore** (dibutuhkan mode B default).
- [ ] **Update tabel curve/feature `se050_driver.cpp` agar jujur** + catat temuan di ADR 0014.

> Tidak memblok Fase 1–2 (software backend). Hasil baru kepakai di Fase 4 (SE backend).

---

## 6. UI / UX — matang, bukan tempelan

Resolution-independent (`canvas.width()/height()` + `aether::theme()` tokens), footer dari
`hintFor(Action)` (tak hardcode), marquee (`SmartLabel`), navigasi `ViewDispatcher` push/pop +
transitions. Semua state dirancang: **loading / empty / error / locked**.

### Inventori layar (jangan ada yang ketinggalan)

| Layar | Isi | Catatan UX |
|---|---|---|
| **Onboarding — gerbang** | "Create new wallet" / "Restore from phrase" | first-run; sebelum ini app terkunci |
| **Create — reveal phrase** | tampilkan 12/24 kata, "sudah dicatat?" | warning jangan screenshot; tombol next di-delay |
| **Create — verify phrase** | minta beberapa kata acak | pastikan user benar mencatat |
| **Restore — entry** | input mnemonic via **VirtualKeyboard** (Plan 23) | validasi checksum BIP39 live |
| **Set PIN** | bikin PIN (gate signing + reveal) | user-presence (§1); auto-lock idle |
| **Unlock** | minta PIN saat app dibuka / lock | fail → throttle; bukan brute-force-able |
| **Account list** | akun per chain + saldo (opsional) + **badge backend** 🔒/⚠ | empty-state "belum ada akun" + add |
| **Receive** | address penuh + **QR** + copy | truncate di list, penuh di sini |
| **Send (opsional in-app)** | form tujuan+jumlah → bangun tx → consent | mayoritas tx dari dApp; ini nilai tambah |
| **SignConsent (modal, sistem)** | §3 — decoded + raw + origin + badge + blind-sign warning | **dimiliki sistem**, bukan app |
| **Connect (modal)** | "Allow [app/origin] lihat wallet?" | PermissionService; anti-phishing origin |
| **Connected apps** | daftar dApp ter-connect + revoke | transparansi & kontrol |
| **Settings** | backup phrase (di balik PIN), ganti PIN, kelola network, **wipe wallet** | wipe = konfirmasi keras |

### Prinsip UX
- **Indikator backend konsisten** (🔒 Secure Element / ⚠️ Software key) muncul di header app **dan** di
  consent modal — user selalu tahu key-nya dijaga apa saat menyetujui.
- **Address**: truncate tengah (`0x1a2b…f9`) di list; penuh + QR di Receive.
- **Anti-phishing**: origin/app id selalu terlihat di connect & sign; warning saat connect pertama.
- **Blind-sign warning** mencolok (bukan teks kecil) saat tx tak ter-decode.
- **Locked state**: setelah idle/auto-lock, app minta PIN sebelum apa pun yang sensitif.
- **PIN bukan sekadar gate UI:** mode C → kunci enkripsi seed **diturunkan dari PIN** (KDF); mode B →
  PIN membuka akses unwrap di SE (auth object). Tanpa ini, dump flash (Secure Boot OFF) bikin seed
  rawan sebatas entropi PIN. Salah PIN → **throttle** (anti brute-force).

---

## 7. Struktur file/module

```text
firmware/core/include/nema/wallet/
  wallet_types.h        # Curve, Chain, NetworkParams, Account, TxPreview, SigningItem, BackendKind
  wallet_backend.h      # IWalletBackend (CUSTODY) — getAddress/sign, NEVER expose key
  chain.h               # IChain (CHAIN) + ChainInfo
  network_registry.h    # data-driven networks (eth/polygon/arbitrum/btc/sol/...)
  wallet_service.h      # orkestrator: backend × chain × consent, multi-account, activeBackendKind()
  wallet_request.h      # WalletRequest envelope (connect/getAccounts/signTx/signMessage) + origin
firmware/core/src/wallet/
  wallet_service.cpp · network_registry.cpp · wallet_request_router.cpp
  backends/ se_backend.cpp · nvs_backend.cpp
  chains/   evm.cpp · bitcoin.cpp · solana.cpp     # RLP/keccak · PSBT/bech32 · base58/ed25519
  hd/       bip32.cpp · bip39.cpp                  # pakai trezor-crypto

firmware/core/include/nema/screens/   wallet_*  (onboarding, unlock, account_list, receive,
                                       sign_consent, connect, connected_apps, settings)
firmware/core/include/nema/apps/wallets_app.h  + src/apps/wallets_app.cpp   # launcher app

firmware/vendor/trezor-crypto/                  # secp256k1/ed25519/nist256p1/bip32/bip39/keccak/base58/bech32 (MIT)
firmware/boards/skyrizz-e32/src/se050_driver.cpp  # ISI TODO: genKey/sign/wrap via Plug&Trust
firmware/platforms/wasm/.../sim_secure_element.h  # lengkapi key ops (dev di browser)

packages/<wallet-bridge>/ (TS)                  # provider shim Fase 7: eip1193/eip6963/walletStandard/btcPsbt + PLP bridge
```
Permission strings (konstanta) dikonsumsi **PermissionService** (Plan 87; key `appId:cap`, persist via
IConfigStore), dideklarasikan app via manifest `needs`: **`wallet.read`** (connect/lihat akun),
**`wallet.sign`** (boleh *meminta* tandatangan; approval aktual tetap consent per-tx, K6),
**`se.raw`** (privileged, internal-only, K7). Katalog konstanta ikut konvensi Plan 87/89 — **terpisah**
dari `caps::Secure` yang tetap **capability hardware** (bukan permission).

---

## 8. Fase implementasi & checklist

### Fase 0 — Spike SE050 (paralel, tidak memblok) — §5
- [ ] (checklist §5)

### Fase 1 — Software crypto core + HD wallet (backend NVS / mode C)
- [ ] Vendor **trezor-crypto** + wiring CMake (host-build & IDF). Subset: secp256k1 (RFC6979 + low-S),
      ed25519, nist256p1, bip32, bip39, keccak, base58, bech32.
- [ ] `wallet_types.h` + HD: BIP39 gen/restore, **BIP32 (secp256k1)** + **SLIP-0010 (Ed25519,
      hardened-only — Solana `m/44'/501'/x'/0'`)**, BIP44 path per chain.
- [ ] **Unit test host** (`firmware/tests/`) — test vector BIP39/BIP32 + known-answer address per chain.
- [ ] `NvsBackend` (mode C): seed terenkripsi (KDF+AES) di `criticalStorage()`.

### Fase 2 — IChain × NetworkRegistry (sumbu chain)
- [ ] `IChain` + `EvmChain`/`BitcoinChain`/`SolanaChain` (address, pathFor, decodeForDisplay,
      buildSigningPayload, encodeSigned, decodeMessage).
- [ ] `NetworkRegistry` data-driven (mainnet + testnet/devnet tiap family).
- [ ] EVM: clear-sign native transfer + ERC-20 transfer; sisanya tandai **blind-sign**.
- [ ] BTC: fee = Σin−Σout butuh amount input di PSBT (witness-utxo); bila absent → fee "unknown" + warn.
- [ ] Test: decode→sign→encode round-trip per chain melawan test vector (termasuk PSBT multi-input).

### Fase 3 — WalletService + dua-sumbu + indikator
- [ ] `IWalletBackend` + `WalletService` (pilih backend via `caps::Secure`+spike; multi-account;
      `activeBackendKind()`).
- [ ] Compose: `signTransaction` = chain.decode → consent → backend.sign → chain.encode.
- [ ] Daftarkan service di container; resolve dari app/JS.

### Fase 4 — SE050 driver (isi TODO) + SeBackend (mode B)
- [ ] Integrasi **Plug & Trust** (APDU/T=1); implement `Se050Driver` genKey/publicKey/sign/verify/
      randomBytes/uniqueId sungguhan.
- [ ] Ekstensi `ISecureElement`: **wrap/unwrap** seed (di belakang `hasFeature()`, jaga kontrak ADR 0005).
- [ ] `SeBackend`: seed di-wrap SE → ciphertext di NVS; sign = unwrap→derive→sign→wipe.
- [ ] Lengkapi `SimSecureElement` (WASM) untuk mode B; verifikasi address/sig **identik** SeBackend↔NvsBackend.

### Fase 5 — Consent & permission (sistem)
- [ ] `SignConsentScreen` (`ScreenMode::Modal`) + publish via GuiService (pola PermissionService).
- [ ] Properti §3: system-owned, decode==sign, physical-only, fail-closed, serialized, origin, blind-warn.
- [ ] `wallet.read` connect (permission persist) vs sign (consent per-tx, tak diingat).

### Fase 6 — App "Wallets" + UI/UX (§6)
- [ ] `WalletsApp : ComponentApp` (`com.palanu.wallets`) + register di `targets/*/main.cpp` + launcher.
- [ ] Onboarding (create/reveal/verify/restore via VirtualKeyboard) + Set PIN + Unlock + auto-lock.
- [ ] Account list (+badge backend), Receive (+QR), Connected apps, Settings (backup/PIN/network/wipe).
- [ ] State loading/empty/error/locked dirancang. Indikator backend konsisten.

### Fase 7 — dApp bridge (native/JS/web)
- [ ] `WalletRequestRouter` + envelope seragam; host binding `nema.wallet.*` (JS/WASM).
- [ ] Web shim (TS): EIP-1193+6963 (EVM), Wallet Standard (SVM), signPsbt (BTC) → PLP transport
      (reqId Plan 88 + auth Plan 74); origin diteruskan ke consent.

### Fase 8 — Cold wallet ↔ Phantom (NEXT GOAL, ekspektasi realistis)
> ⚠️ **Bukan "tinggal connect BLE".** Phantom hanya bicara HW wallet yang sudah diintegrasikan
> (Ledger via APDU spesifik); tak bisa auto-detect device BLE sembarangan.
- [ ] Riset 2 opsi: (a) **emulasi protokol Ledger** (effort besar, tiru APDU), (b) **Wallet Standard
      via web shim** (shim muncul sebagai wallet, bukan Phantom detect device langsung).
- [ ] Pilih jalur + plan terpisah. Dikerjakan **setelah core wallet (Fase 1–7) selesai.**

---

## 9. Definition of Done

1. **STATE.md** — baris "Wallet / Secure Element": `build` → `sim ✓` → `HW-verified` seiring fase.
2. **docs/feats/** — `docs/feats/wallet.md` (cara kerja wallet, dua-sumbu, backend, consent, dApp flow *sekarang*).
3. **docs/decisions/**
   - **ADR 0014** — key-mode (Seed-in-SE wrapped) + model ancaman + consent dua-tingkat + nama `wallet`.
     **Tulis sebelum Fase 4.**
   - **ADR 0015** — arsitektur dua/tiga-sumbu (Custody × Chain × Consent), network=data, bridge
     satu-core. **Tulis sebelum Fase 3.**
   - ADR tambahan bila Plug & Trust membawa trade-off besar (footprint RAM/flash, lisensi).
4. **Checklist** plan ini dicentang seiring kerjaan.
5. **Commit** conventional (`feat(wallet):`, `feat(se):`, `fix(se):`, `docs:`).
6. **Keamanan:** indikator backend jujur; **tidak ada klaim "Ledger-class"**; consent fail-closed &
   physical-only; build dev testnet-only sampai Secure Boot ON.
