#pragma once
#include "nema/wallet/wallet_backend.h"
#include "nema/wallet/hd_wallet.h"
#include "nema/wallet/seed_store.h"
#include "nema/hal/secure_element.h"
#include <string>

// SeBackend — mode-B custody (Plan 94, ADR 0014). The BIP39 seed is encrypted under a
// PIN-derived key AND wrapped by the secure element's device-bound key, so the stored
// blob needs BOTH the PIN and the physical chip to recover (defence in depth). Signing
// runs in software (HdWallet) after the SE unwraps the seed; the SE guards the seed at
// rest. kind() == SecureElement → the UI shows the 🔒 "Secure Element" indicator.

namespace nema::wallet {

class SeBackend : public IWalletBackend {
public:
    SeBackend(ISecureElement& se, ISeedStore& store, uint8_t slot = 0)
        : se_(se), store_(store), slot_(slot) {}

    // ── IWalletBackend ──
    BackendKind kind() const override { return BackendKind::SecureElement; }
    bool ready() const override { return hd_.ready(); }
    bool publicKey(const DerivationPath& p, Curve c, PubKey& out) override {
        return hd_.publicKey(p, c, out);
    }
    bool sign(const DerivationPath& p, Curve c, const uint8_t* payload, size_t n,
              bool prehashed, Signature& out) override {
        return hd_.sign(p, c, payload, n, prehashed, out);
    }

    // ── Lifecycle ──
    bool hasWallet() const override { return store_.exists(); }
    bool create(const std::string& mnemonic, const std::string& passphrase,
                const std::string& pin) override;
    bool unlock(const std::string& pin) override;
    void lock() override { hd_.lock(); }
    void wipe() override { hd_.lock(); store_.erase(); }

    // Whether `se` supports the device-bound sealing mode B requires.
    static bool supportedBy(ISecureElement& se) { return se.hasFeature(SeFeature::SecureStore); }

private:
    ISecureElement& se_;
    ISeedStore&     store_;
    uint8_t         slot_;
    HdWallet        hd_;
};

}  // namespace nema::wallet
