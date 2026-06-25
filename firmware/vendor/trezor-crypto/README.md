# trezor-crypto (vendored)

Wallet crypto core for **Plan 94** (crypto wallet + secure element). Software crypto
that works on every board, in the simulator, and for Ed25519 regardless of secure-element
support — per **ADR 0005** (the SE is an *optional* key-custody backend, not the
foundation) and **ADR 0015** (the `IWalletBackend` software backend).

## Provenance

- Source: `trezor-firmware/crypto/` — https://github.com/trezor/trezor-firmware
- Commit: `44cc50e7de33ea5aca3e6fd8e3034587a34a8a6b` (2026-06-25)
- License: **MIT** (see `LICENSE`, `AUTHORS`).

This is a **curated subset**, not the whole `crypto/` tree. Excluded: monero, cardano,
nem, shamir/slip39, chacha20poly1305, zkp (libsecp256k1-zkp), gui/fuzzer/tests/tools.
The compiled file list is the `TZ_SRCS` in `CMakeLists.txt`.

## What it provides

- **secp256k1** + **nist256p1** ECDSA — RFC6979 deterministic nonces, low-S.
- **ed25519** EdDSA + **SLIP-0010** derivation (Solana).
- **BIP32** (secp256k1) / **BIP39** mnemonic+seed / BIP44 paths.
- Hashes: keccak/sha3, sha2 (256/512), ripemd160, blake (pulled in by `hasher.c`).
- Encodings: base58, bech32 (`segwit_addr`).
- AES (`aes/`) — only because `bip32.c`'s ECIES helpers reference it.

## ⚠️ Platform must provide the RNG

trezor-crypto declares but does **not** define `random32()` and `random_buffer()` —
they are platform hooks. A consumer that links this library **must** supply both:

- **Firmware:** back them with the SE050 TRNG (`ISecureElement::randomBytes`) or `esp_random`.
- **Host tests / sim:** a stub (e.g. `arc4random`) — see `firmware/tests/wallet_crypto_test.cpp`.

Omitting them is an intentional link error, not a bug.

## Verification

`firmware/tests/wallet_crypto_test.cpp` checks address derivation against authoritative
vectors: ETH (BIP44), BTC (BIP84 bech32), and Ed25519 (SLIP-0010 spec vector 1). Run via
the host build (`ctest`).

## Updating

Re-fetch the same files at a newer commit, keep `TZ_SRCS` in sync, and re-run the test.
Do not hand-edit vendored sources.
