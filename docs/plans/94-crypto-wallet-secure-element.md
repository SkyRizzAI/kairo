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

#### Cakupan chain & network (scope eksplisit)

Tiga `IChain` (kode) menampung family berikut. EVM = **satu driver**, semua network di bawahnya cuma
beda `chainId` (= **data**, +1 baris per network, nol kode tambahan):

| Family | Driver | Kurva / derivasi | Address | Tx | Network target MVP |
|---|---|---|---|---|---|
| **EVM** | `EvmChain` | secp256k1 / BIP32 `m/44'/60'` | keccak256 → `0x…` | EIP-1559 + legacy (RLP) | **Ethereum (1), BNB Smart Chain (56), Polygon (137), Arbitrum (42161), Optimism (10), Base (8453), Avalanche C (43114)** + Sepolia testnet |
| **Bitcoin** | `BitcoinChain` | secp256k1 / BIP32 `m/84'/0'` | bech32 P2WPKH (`bc1…`) | PSBT (multi-input) | mainnet + testnet |
| **Solana (SVM)** | `SolanaChain` | Ed25519 / **SLIP-0010** `m/44'/501'` | base58 | message Solana | mainnet-beta + devnet |

**Batas "bisa" (set ekspektasi):**
- **Status:** masih plan — chain jadi nyata setelah **Fase 1 (crypto+HD)** + **Fase 2 (3 driver)**.
- **Device = signer, bukan broadcaster.** Kirim tx ke jaringan = urusan app/dApp (pola Trezor/MetaMask);
  device tak menyimpan RPC node.
- **Clear-sign vs blind-sign:** v1 clear-sign **transfer native + ERC-20 transfer** (tampil to/amount/fee);
  contract call rumit (swap/approve) → **blind-sign + warning**. Cakupan decoder bertambah bertahap.
- Nambah EVM network baru (mis. zkSync, Linea) = **+1 baris di NetworkRegistry**, tanpa sentuh kode chain.

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
- [x] Vendor **trezor-crypto** (`firmware/vendor/trezor-crypto/`, MIT, commit 44cc50e7) + wiring CMake
      dual-build (host `add_subdirectory` ✓ verified; IDF `idf_component_register` ditulis). Subset:
      secp256k1 (RFC6979 + low-S), ed25519, nist256p1, bip32, bip39, keccak/sha3, base58, bech32 + aes
      (ECIES bip32). RNG = platform hook (random32/random_buffer; test pakai stub).
- [x] **Kontrak `wallet_types.h`** (Curve/DerivationPath/SigningItem/NetworkParams/Account/preview) —
      header kompilasi bersih.
- [x] **Unit test host** (`firmware/tests/wallet_crypto_test.cpp`, lewat `ctest`) — vektor otoritatif
      LULUS: **ETH** BIP44 `m/44'/60'/0'/0/0`, **BTC** BIP84 bech32 `m/84'/0'/0'/0/0`, **SOL** SLIP-0010
      ed25519 spec vector 1. Membuktikan bip39+bip32+secp256k1+ed25519+keccak+bech32 benar.
- [x] **`HdWallet` wrapper** C++ (`core/wallet/hd_wallet.{h,cpp}`) — bungkus trezor: generate/validate
      mnemonic, unlockFromMnemonic/Seed, publicKey, sign(path,curve,prehashed), lock/wipe. Header tak
      bocorkan trezor ke core. Test `hd_wallet_test` LULUS (derivasi + secp256k1/ed25519 sign **+verify**
      + tolak prehashed-misuse + lock-wipe).
- [x] `NvsBackend` (mode C, `core/wallet/nvs_backend.cpp` + `seed_store.h`): seed terenkripsi
      **PBKDF2-HMAC-SHA256 → AES-256-CBC**, kunci diturunkan dari PIN, blob `[salt][iv][ct]` via
      `ISeedStore` (firmware → `criticalStorage()`; test → in-memory). Test `nvs_backend_test` LULUS
      (roundtrip, **wrong-PIN ditolak**, persistensi lintas-instance, wipe).
- [x] **Build wiring:** `trezor-crypto` di-link ke `nema_core` (host PUBLIC + IDF REQUIRES); **seluruh
      17 host test LULUS** (14 lama tak rusak + 3 wallet baru).

### Fase 2 — IChain × NetworkRegistry (sumbu chain)
- [x] `NetworkRegistry` data-driven (`network_registry.cpp`) — 12 network (EVM ×8: eth/bnb/polygon/
      arbitrum/optimism/base/avalanche/sepolia, BTC ×2, SOL ×2). Tambah network EVM = +1 baris.
- [x] **RLP codec** (`chains/rlp.{h,cpp}`) — encode/decode string+list; test `rlp_test` LULUS (vektor spec RLP).
- [x] **`EvmChain`** (`chains/evm.{h,cpp}`) — satu driver semua network EVM: address (keccak), pathFor
      `m/44'/60'/0'/0/i`, decodeForDisplay (clear-sign native transfer + **ERC-20 transfer** + blind-sign
      fallback contract call), buildSigningPayload (EIP-155 & EIP-1559 → keccak preimage), encodeSigned
      (v=recid+35+2·chainId legacy / type-2). Test `evm_chain_test` LULUS terhadap **vektor EIP-155
      resmi** (signing hash + signed-tx byte-identik) + address `0x9858…da94`.
- [x] **`SolanaChain`** (`chains/solana.{h,cpp}`) — base58 raw, pathFor `m/44'/501'/i'/0'`, decode
      message (SystemProgram Transfer clear-sign + blind-sign sisanya), sign pesan utuh (Ed25519),
      encodeSigned wire (count|sig|message). Test `solana_chain_test` LULUS (base58 vektor zeros→ones,
      decode Transfer 1 SOL, sign **+verify** Ed25519, wire format).
- [x] **`BitcoinChain`** (`chains/bitcoin.{h,cpp}`) — **address bech32 P2WPKH + pathFor `m/84'/0'/0'/0/i`
      LENGKAP & teruji** (`bitcoin_chain_test` LULUS vektor BIP84 `bc1qcr8te4…306fyu` + HRP testnet `tb1`).
- [ ] **BTC tx-signing follow-up** (BIP143 sighash + PSBT) — fail-closed sekarang; wajib diverifikasi
      lawan vektor spec BIP143 sebelum sign mainnet. Bukan penghalang app (receive jalan; kirim nyusul).
- [x] EVM/SOL: decode→sign→encode round-trip teruji lawan vektor (EIP-155 / Transfer message).

### Fase 3 — WalletService + dua-sumbu + indikator
- [x] `WalletService` (`wallet_service.{h,cpp}`) — registry chain by-family, `deriveAddress`,
      `previewTransaction`, `signTransaction`, `activeBackendKind()` (→ indikator UI).
- [x] Compose: `signTransaction` = chain.decode → **consent (Confirm callback, fail-closed)** →
      backend.sign(tiap item) → chain.encode. Consent di-inject (Fase 5 = modal sistem; test = auto).
- [x] Test `wallet_service_test` LULUS — derive ETH/BTC/SOL via satu service, preview, **signature
      EVM tertanam terverifikasi untuk akun**, **reject→fail-closed**, Solana sign. **22/22 host test hijau.**
- [ ] Daftarkan service di container + resolve dari app/JS — saat wiring app (Fase 6).

### Fase 4 — SE050 driver (isi TODO) + SeBackend (mode B)
- [x] Ekstensi `ISecureElement`: **wrap/unwrap** (di belakang `hasFeature(SecureStore)`, default false —
      jaga kontrak ADR 0005).
- [x] **`SoftSecureElement`** (`soft_secure_element.{h,cpp}`) — SE software device-bound (AES-256 kunci
      internal) untuk host+sim; melengkapi cerita SimSecureElement untuk dev mode B tanpa chip.
- [x] **`SeBackend`** (mode B, `se_backend.cpp`): seed → PIN-encrypt (PBKDF2→AES) → **SE.wrap (device-bound)**
      → simpan; unlock = unwrap→PIN-decrypt→MAGIC-check→derive. Butuh **PIN _dan_ chip**. Test
      `se_backend_test` LULUS (roundtrip, wrong-PIN, **device-bound: chip beda → gagal**, indikator
      🔒 SecureElement). Address identik dgn NvsBackend (`0x9858…da94`).
- [x] **Auto-selection ter-wire** (Fase 7): `WalletVault` jadi SE-aware (wrap/unwrap seed pakai
      `ISecureElement` opsional → `kind()` SecureElement vs Software), dan `bootWalletSystem` **resolve
      `ISecureElement` dari container + pilih otomatis** kalau `hasFeature(SecureStore)` (tanpa branching
      board — tanya chip-nya). skyrizz sudah `registerAs<ISecureElement>(Se050)` + `caps::Secure`
      (gated `present()`). `wallet_vault_test` LULUS untuk path mode-B (device-binding, 🔒 indikator).
      → **Begitu Se050Driver implement `SecureStore`, wallet auto-upgrade ke mode B, nol perubahan kode.**
- [ ] **Se050Driver Plug&Trust asli** (APDU/T=1: genKey/sign/wrap/unwrap di chip) — **hardware-gated TODO**
      (butuh SE050 fisik + middleware NXP + spike Fase 0). Sampai itu, skyrizz `hasFeature(SecureStore)=false`
      → wallet jalan software (mode C, PIN-encrypted) — indikator "Software key" jujur.

### Fase 5 — Consent & permission (sistem)
- [x] **`WalletConsentService`** (`wallet_consent_service.{h,cpp}`) — plumbing consent system-owned ala
      PermissionService: `requestSign` block worker-thread sampai GUI resolve; `guiTick` push layar via
      ScreenFactory; `confirmFor()` adapter ke `WalletService::Confirm`. Properti §3 terpenuhi di level
      service: **system-owned, fail-closed** (tanpa factory→reject), **serialized** (1 prompt), **origin**
      (anti-phishing), **decode==sign** (preview dari WalletService), **resolve hanya dari layar**
      (tombol fisik). Test `wallet_consent_test` LULUS (approve/reject/fail-closed).
- [x] Permission strings `wallet.read`/`wallet.sign`/`se.raw` (perms:: di `wallet_types.h`) — dikonsumsi
      PermissionService existing; signing tetap consent per-tx (tak diingat).
- [ ] **`SignConsentScreen`** (`ScreenMode::Modal`, visual) + bind ScreenFactory + blind-sign warning —
      di display layer, dikerjakan bareng app (Fase 6).

### Fase 6 — App "Wallets" + UI/UX (§6)
- [x] **`WalletController`** (`wallet_controller.{h,cpp}`) — otak app: state machine
      NoWallet/Locked/Unlocked, generate/validate mnemonic, createWallet(=restore), unlock/lock/wipe,
      `accounts()` (derive address+label per network), `backendKind()` (indikator). Lifecycle diangkat ke
      `IWalletBackend` (backend-agnostic). Test `wallet_controller_test` LULUS (**11/11 wallet suite hijau**).
- [x] **`WalletsApp : ComponentApp`** (`apps/wallets_app.{h,cpp}`) — compile + **link** + identity terverifikasi
      (`wallets_app_test`), **register di 3 target** (skyrizz-e32/dev-board/wasm) → muncul di launcher "Apps".
- [x] Layar: **Onboard** (Create/Restore) · **Reveal** (12 kata) · **Restore** (VirtualKeyboard) · **SetPin**
      (keyboard password) · **Locked** (unlock PIN) · **Accounts** (list + badge 🔒/⚠️) · **Receive** (address)
      · **Settings** (Wipe + Dialog konfirmasi). `AppStorageSeedStore`→`criticalStorage()`.
- [x] Backend default **NvsBackend** (⚠️ Software key) — `SeBackend` swap-in saat SE wrap (Plug&Trust) ada.
- [ ] **`SignConsentScreen`** visual + bind `WalletConsentService` ScreenFactory — nunggu trigger (Fase 7 dApp / in-app send).
- [ ] **Validasi visual on-device** (flash) — logika 12/12 host-test hijau; tampilan butuh perangkat.

### Fase 7 — dApp bridge (native/JS/web)
- [x] **`WalletSystem`** (`wallet_system.{h,cpp}`) — stack wallet **tunggal/shared**, register di container
      saat boot (`bootWalletSystem(rt)` di 3 target). WalletsApp + custom app pakai wallet yang SAMA.
- [x] **Host binding `nema.wallet.*`** (IDL `api/wallet.pidl` → codegen → `nema_host_impl.cpp`):
      `networks/ready/address/signMessage/signTransaction`, gated `wallet.read`/`wallet.sign` + consent.
      **Private key tak pernah ke-expose** — cuma address + signature. + `WalletService.signMessage`
      + `IChain.messageSigningItem` (EVM EIP-191, SOL raw).
- [x] **`SignConsentScreen`** (`screens/sign_consent_screen.{h,cpp}`) — modal trusted-display saat sign
      dari dApp; di-wire di `GuiService` (ScreenFactory + guiTick, pola PermissionScreen). Fail-closed.
- [x] **Contoh `examples/web3-test` → `Web3 Test.papp`** (build ✓) — pick network, show address,
      sign message/tx. `app:build:web3-test`.
- [ ] **Web shim (TS)**: EIP-1193+6963 (EVM), Wallet Standard (SVM), signPsbt (BTC) → PLP transport
      (browser dApp) — **next goal** (Fase 8, bareng Phantom).

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
