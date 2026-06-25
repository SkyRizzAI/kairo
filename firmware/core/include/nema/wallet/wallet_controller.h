#pragma once
#include "nema/wallet/wallet_vault.h"
#include "nema/wallet/wallet_service.h"
#include "nema/wallet/hd_wallet.h"

#include <string>
#include <vector>

// WalletController — the app's brain (Plan 94). A thin, host-testable layer over
// WalletVault (multiple wallets, one PIN) + WalletService (addresses/signing for the
// active wallet). The ComponentApp screens call into this; logic lives here.

namespace nema::wallet {

enum class WalletState { NoWallet, Locked, Unlocked };

class WalletController {
public:
    struct AccountView {
        std::string networkId;
        std::string label;
        std::string address;
    };

    WalletController(WalletVault& vault, WalletService& svc) : vault_(vault), svc_(svc) {}

    WalletState state() const {
        if (!vault_.hasWallet()) return WalletState::NoWallet;
        return vault_.unlocked() ? WalletState::Unlocked : WalletState::Locked;
    }
    BackendKind backendKind() const { return svc_.activeBackendKind(); }

    // ── Onboarding / mnemonic ──
    static bool generateMnemonic(bool words24, std::string& out) {
        return HdWallet::generateMnemonic(words24 ? 256 : 128, out);
    }
    static bool validateMnemonic(const std::string& m) { return HdWallet::validateMnemonic(m); }

    // First wallet (sets the vault PIN).
    bool createFirstWallet(const std::string& mnemonic, const std::string& pin) {
        std::string id;
        return vault_.createFirst(mnemonic, pin, id);
    }
    // Additional wallet (vault already unlocked).
    bool addWallet(const std::string& mnemonic) {
        std::string id;
        return vault_.addWallet(mnemonic, id);
    }

    // ── Multiple wallets ──
    const std::vector<WalletMeta>& wallets() const { return vault_.wallets(); }
    const std::string& activeId() const { return vault_.activeId(); }
    bool selectWallet(const std::string& id) { return vault_.select(id); }
    bool removeWallet(const std::string& id) { return vault_.remove(id); }

    // ── Accounts within the active wallet (one seed → many accounts, BIP44 index) ──
    uint32_t accountCount() const { return vault_.activeAccountCount(); }
    bool addAccount() { return vault_.addAccount(); }
    // Owner-initiated key export: returns the hex private key for (account, network).
    bool exportPrivateKey(uint32_t accountIndex, const char* networkId, std::string& hexOut);

    // ── Lock/unlock ──
    bool unlock(const std::string& pin) { return vault_.unlock(pin); }
    void lock() { vault_.lock(); }
    void wipe() { vault_.wipe(); }

    // Addresses for the given networks at account `index` (empty unless Unlocked).
    std::vector<AccountView> accounts(const std::vector<std::string>& networkIds, uint32_t index = 0);

    static std::vector<std::string> defaultNetworks() {
        return {"eth-mainnet", "btc-mainnet", "sol-mainnet"};
    }

private:
    WalletVault&   vault_;
    WalletService& svc_;
};

}  // namespace nema::wallet
