#pragma once
#include "nema/app/component_app.h"
#include "nema/ui/node.h"
#include "nema/ui/virtual_keyboard.h"
#include "nema/wallet/wallet_controller.h"

#include <string>
#include <vector>

// WalletsApp — built-in "Wallets" launcher app (Plan 94). A ComponentApp whose screens
// are thin views over the SHARED WalletController, resolved from the container (the same
// system wallet that custom apps reach via nema.wallet.*). The wallet stack itself is
// owned by WalletSystem (registered at boot), not by this app.

namespace nema {

class WalletsApp : public ComponentApp {
public:
    const char* id() const override { return "com.palanu.wallets"; }
    const char* name() const override { return "Wallets"; }
    // "System" → launcher-level app (hardcoded launcher entry; hidden from the Apps
    // list, which is for custom apps). Same convention as BadUSB.
    const char* category() const override { return "System"; }
    bool fullscreen() const override { return false; }

protected:
    void onStart(AppContext& ctx) override;
    aether::ui::UiNode* build(aether::ui::NodeArena& arena, AppContext& ctx) override;
    aether::ui::UiNode* buildModal(aether::ui::NodeArena& arena, AppContext& ctx) override;
    bool onKey(Key k, AppContext& ctx) override;
    bool capturesInput() const override;
    bool drawRaw(Canvas& c, AppContext& ctx) override;
    size_t arenaCapacity() const override { return 512; }

private:
    // Home is the landing menu (adapts to wallet state); the passphrase reveal is only
    // reached by explicitly choosing "Create new wallet".
    enum class State {
        Home, WalletList, AddChoose, Reveal, Restore, SetPin, Locked,
        Accounts,     // list of accounts (Account 1..N) in the active wallet
        AcctDetail,   // one account's addresses per chain + export
        Receive,      // full address of one chain
        ExportPin,    // PIN re-auth before revealing a private key
        ExportKey,    // revealed private key (after confirm + PIN)
        WipePin,      // PIN re-auth before wiping all wallets
        Settings, About
    };

    void refreshAccounts(uint32_t accountIndex = 0);

    // onPress trampolines (userdata = this, or a RowCtx*).
    static void cbCreate(void*);
    static void cbRestore(void*);
    static void cbRevealContinue(void*);
    static void cbMyWallets(void*);     // Home → wallet list (unlock first if locked)
    static void cbAddChoose(void*);     // wallet list → Create/Import chooser
    static void cbSelectWallet(void*);     // pick a wallet from the list → its accounts
    static void cbAddAccount(void*);       // derive one more account in the active wallet
    static void cbOpenAccountIdx(void*);   // open an account (index) → its addresses
    static void cbShowKey(void*);          // request private-key export (opens confirm)
    static void cbExportConfirm(void*);    // confirmed → reveal the key
    static void cbExportCancel(void*);
    static void cbOpenSettings(void*);
    static void cbAbout(void*);
    static void cbOpenAccount(void*);      // open one chain address → Receive
    static void cbWipeConfirm(void*);
    static void cbWipeCancel(void*);

    State state_ = State::Home;

    wallet::WalletController* ctl_ = nullptr;   // shared system wallet (resolved at onStart)

    aether::ui::VirtualKeyboard kb_;
    aether::ui::ScrollState     scroll_{};

    std::string revealMnemonic_;   // generated phrase being shown
    std::string pendingMnemonic_;  // phrase awaiting a PIN (create or restore)
    std::string pendingPin_;       // PIN captured BEFORE the phrase reveal (create flow)
    bool        pinForCreate_ = false; // SetPin entered from Create → go to Reveal, not commit
    std::string errorMsg_;
    bool        wantWipe_ = false; // drives the confirm modal

    std::vector<wallet::WalletController::AccountView> accounts_;
    std::vector<std::string> rowVal_;  // persistent truncated-address strings (row value)
    std::vector<std::string> rowSym_;  // persistent chain tickers ETH/BTC/SOL (row label)
    struct RowCtx { WalletsApp* self; int idx; };
    std::vector<RowCtx> rowCtx_;        // chain-address rows (one account)
    std::vector<RowCtx> walletRowCtx_;  // wallet rows (wallet list)
    std::vector<RowCtx> acctRowCtx_;    // account rows (account list)
    int      selected_ = 0;             // chain index within the open account
    uint32_t acctIdx_  = 0;             // open account's BIP44 index
    std::string exportHex_;             // revealed private key (transient)
    std::string exportNet_;             // network id whose key is being exported
    std::string exportTitle_;           // "Private key — Ethereum" subheader
    bool        wantExport_ = false;    // drives the export-confirm modal
    std::vector<std::string> acctLabels_;   // "Account 1".. (persistent row labels)
    std::string              acctTitle_;    // "Account N" title for AcctDetail
    std::vector<std::string> exportLines_;  // private key split into scrollable rows
    aether::ui::DialogButton wipeBtns_[2];  // persistent (Dialog references after build)

    char wordBuf_[24][24];  // "1. abandon" rows for the reveal screen
};

}  // namespace nema
