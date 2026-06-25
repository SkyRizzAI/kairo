#pragma once
#include "nema/wallet/wallet_backend.h"
#include "nema/wallet/hd_wallet.h"
#include "nema/hal/secure_element.h"

#include <string>
#include <vector>

// WalletVault — multi-wallet custody (Plan 94, Fase 6+). The Phantom/MetaMask model:
// ONE device PIN guards a vault holding MANY wallets, each its own BIP39 seed. One
// wallet is "active" at a time; the vault implements IWalletBackend by delegating
// publicKey()/sign() to the active wallet's HdWallet, so WalletService is unchanged.
//
// Each seed is encrypted with a PIN-derived key (PBKDF2-HMAC-SHA256 → AES-256-CBC) and
// stored under its own key in an IKvStore; a plaintext index lists the wallets + the
// active id. Adding a wallet while unlocked reuses the in-RAM PIN (no re-prompt).
//
// Secure-element mode (ADR 0014 mode B): if constructed with an ISecureElement that
// supports device-bound sealing (SeFeature::SecureStore), each PIN-encrypted seed blob
// is ADDITIONALLY wrapped by the chip's device-bound key before storage — so recovery
// needs BOTH the PIN and the physical chip. kind() then reports SecureElement (🔒).
// Without such a chip the vault stays pure software (mode C, ⚠️). The choice is made by
// WalletSystem at boot from the board's capabilities — the vault logic is identical.

namespace nema::wallet {

// Multi-key blob store (firmware: AppStorage::critical(); tests: in-memory).
struct IKvStore {
    virtual ~IKvStore() = default;
    virtual bool put(const char* key, const uint8_t* data, size_t n) = 0;
    virtual bool get(const char* key, std::vector<uint8_t>& out) = 0;
    virtual bool has(const char* key) const = 0;
    virtual void del(const char* key) = 0;
};

struct WalletMeta {
    std::string id;          // short random hex
    std::string label;       // "Wallet 1", ...
    uint32_t    accounts = 1; // BIP44 account count (one seed → many accounts, like Phantom)
};

class WalletVault : public IWalletBackend {
public:
    // se = optional device-bound sealing chip (mode B). nullptr → software (mode C).
    // Pass only an SE that hasFeature(SecureStore); WalletSystem enforces this.
    explicit WalletVault(IKvStore& store, ISecureElement* se = nullptr, uint8_t seSlot = 0);

    // ── IWalletBackend: custody of the ACTIVE wallet ──
    BackendKind kind() const override {
        return se_ ? BackendKind::SecureElement : BackendKind::Software;
    }
    bool ready() const override { return unlocked_ && hd_.ready(); }
    bool publicKey(const DerivationPath& p, Curve c, PubKey& out) override {
        return hd_.publicKey(p, c, out);
    }
    bool sign(const DerivationPath& p, Curve c, const uint8_t* payload, size_t n,
              bool prehashed, Signature& out) override {
        return hd_.sign(p, c, payload, n, prehashed, out);
    }
    // Lifecycle mapped to vault semantics:
    bool hasWallet() const override { return !metas_.empty(); }     // any wallet exists
    bool create(const std::string& mnemonic, const std::string& /*passphrase*/,
                const std::string& pin) override {                   // creates the FIRST wallet
        std::string id;
        return createFirst(mnemonic, pin, id);
    }
    bool unlock(const std::string& pin) override;
    void lock() override;
    void wipe() override;   // erase ALL wallets

    // ── Multi-wallet API ──
    bool unlocked() const { return unlocked_; }
    const std::vector<WalletMeta>& wallets() const { return metas_; }
    const std::string& activeId() const { return activeId_; }

    // First wallet — sets the vault PIN. Fails if a wallet already exists.
    bool createFirst(const std::string& mnemonic, const std::string& pin, std::string& outId);
    // Additional wallet — vault must be unlocked; reuses the in-RAM PIN. Becomes active.
    bool addWallet(const std::string& mnemonic, std::string& outId);
    // Make `id` the active wallet (decrypt its seed into the HD engine).
    bool select(const std::string& id);
    // Delete a wallet; if it was active, another becomes active (or none remain).
    bool remove(const std::string& id);

    // ── Accounts within the active wallet (BIP44 index, like Phantom) ──
    uint32_t activeAccountCount() const;   // ≥1
    bool addAccount();                      // derive one more account index, persist

    // Owner-initiated private-key export for the active wallet (PIN-gated at the UI).
    // Delegates to the active HD engine; never exposed via IWalletBackend.
    bool exportPrivateKey(const DerivationPath& path, Curve curve, Bytes& out) {
        return unlocked_ && hd_.privateKey(path, curve, out);
    }

private:
    bool loadIndex();
    bool saveIndex();
    bool encryptSeed(const uint8_t seed[64], const std::string& pin, std::vector<uint8_t>& out);
    bool decryptSeed(const std::vector<uint8_t>& blob, const std::string& pin, uint8_t seed[64]);
    std::string genId();
    std::string walletKey(const std::string& id) const { return "w." + id; }

    IKvStore&               store_;
    ISecureElement*         se_     = nullptr;  // device-bound sealing (mode B); null = software
    uint8_t                 seSlot_ = 0;
    std::vector<WalletMeta> metas_;
    std::string             activeId_;
    std::string             pin_;        // held in RAM only while unlocked
    bool                    unlocked_ = false;
    HdWallet                hd_;         // active wallet's HD engine
};

}  // namespace nema::wallet
