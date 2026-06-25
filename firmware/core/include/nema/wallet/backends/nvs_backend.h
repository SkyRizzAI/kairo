#pragma once
#include "nema/wallet/wallet_backend.h"
#include "nema/wallet/hd_wallet.h"
#include "nema/wallet/seed_store.h"
#include <string>

// NvsBackend — software custody backend (Plan 94, ADR 0014 mode C).
//
// Stores the BIP39 seed encrypted with a PIN-derived key (PBKDF2-HMAC-SHA256 →
// AES-256-CBC) in an ISeedStore. Signing runs in software via HdWallet; the seed
// only exists in RAM while unlocked. kind() == Software → the UI shows the ⚠️
// "Software key" indicator. This is the fallback on boards without a usable secure
// element; the PIN is a real cryptographic gate (it derives the encryption key),
// not just a UI lock.

namespace nema::wallet {

class NvsBackend : public IWalletBackend {
public:
    explicit NvsBackend(ISeedStore& store) : store_(store) {}

    // ── IWalletBackend ──
    BackendKind kind() const override { return BackendKind::Software; }
    bool ready() const override { return hd_.ready(); }
    bool publicKey(const DerivationPath& p, Curve c, PubKey& out) override {
        return hd_.publicKey(p, c, out);
    }
    bool sign(const DerivationPath& p, Curve c, const uint8_t* payload, size_t n,
              bool prehashed, Signature& out) override {
        return hd_.sign(p, c, payload, n, prehashed, out);
    }

    // ── Lifecycle (driven by WalletService / onboarding UI) ──
    bool hasWallet() const override { return store_.exists(); }

    // Create from a validated mnemonic: derive the seed (mnemonic + passphrase),
    // encrypt it under `pin`, persist, and unlock. The passphrase is baked into the
    // stored seed, so unlock() later needs only the PIN.
    bool create(const std::string& mnemonic, const std::string& passphrase,
                const std::string& pin) override;

    // Decrypt the stored seed with `pin` and unlock the HD engine.
    // Returns false on wrong PIN, corrupt blob, or no wallet.
    bool unlock(const std::string& pin) override;

    void lock() override { hd_.lock(); }
    void wipe() override { hd_.lock(); store_.erase(); }

private:
    ISeedStore& store_;
    HdWallet    hd_;
};

}  // namespace nema::wallet
