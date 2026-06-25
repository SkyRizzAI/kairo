#pragma once
#include "nema/wallet/wallet_types.h"

// HdWallet — software HD key engine (Plan 94, Fase 1). The reusable core that BOTH
// custody backends sit on: NvsBackend owns one directly; SeBackend feeds it the seed
// after the SE050 unwraps it. Knows nothing about chains or storage — just
// seed → BIP32/SLIP-0010 derive → sign. Holds the 64-byte BIP39 seed in RAM and
// wipes it on lock()/destruction (memzero).
//
// This header pulls in NO crypto library — the trezor-crypto dependency is confined
// to hd_wallet.cpp, so it does not leak into core's public include surface.

namespace nema::wallet {

class HdWallet {
public:
    HdWallet() = default;
    ~HdWallet();
    HdWallet(const HdWallet&) = delete;
    HdWallet& operator=(const HdWallet&) = delete;

    // ── Mnemonic helpers (static; need the platform RNG for generate) ──
    // strengthBits: 128 (12 words), 160, 192, 224, 256 (24 words).
    static bool generateMnemonic(int strengthBits, std::string& out);
    static bool validateMnemonic(const std::string& mnemonic);

    // ── Unlock (load the seed into RAM) ──
    bool unlockFromMnemonic(const std::string& mnemonic, const std::string& passphrase = "");
    bool unlockFromSeed(const uint8_t* seed, size_t n);  // n must be 64 (e.g. after SE unwrap)
    bool ready() const { return ready_; }
    void lock();                                          // wipe seed from RAM

    // Public key at `path` for `curve`:
    //   secp256k1 → 33-byte compressed SEC1; ed25519 → 32-byte raw key.
    bool publicKey(const DerivationPath& path, Curve curve, PubKey& out) const;

    // Sign one payload (see SigningItem convention):
    //   secp256k1: payload = 32-byte digest, prehashed=true  → r||s||recid (65 bytes).
    //   ed25519:   payload = whole message,  prehashed=false → 64 bytes.
    // Returns false if locked, on bad arguments, or on signer error. Never returns key material.
    bool sign(const DerivationPath& path, Curve curve,
              const uint8_t* payload, size_t n, bool prehashed, Signature& out) const;

    // Export the 32-byte private key at `path`. SECURITY: this is the ONE place a key
    // leaves the engine — exposed only for an explicit, owner-initiated, PIN-gated
    // "show/export private key" action in the wallet UI. It is NOT reachable through
    // IWalletBackend, so dApps/the signing API can never obtain it.
    bool privateKey(const DerivationPath& path, Curve curve, Bytes& out) const;

private:
    uint8_t seed_[64] = {0};
    bool    ready_    = false;
};

} // namespace nema::wallet
