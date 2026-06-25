#include "nema/apps/wallets_app.h"

#include "nema/app/app_context.h"
#include "nema/runtime.h"
#include "nema/service/service_container.h"
#include "nema/ui/canvas.h"
#include "nema/ui/widgets.h"

#include <cstdio>
#include <cstring>

namespace nema {

using namespace aether::ui;
using wallet::WalletController;

namespace {

void linkChildren(UiNode* parent, const std::vector<UiNode*>& kids) {
    UiNode* prev = nullptr;
    for (auto* k : kids) {
        if (!k) continue;
        if (!parent->firstChild) parent->firstChild = k;
        else prev->nextSibling = k;
        prev = k;
    }
}

std::string truncAddr(const std::string& a) {
    if (a.size() <= 14) return a;
    return a.substr(0, 8) + "…" + a.substr(a.size() - 4);
}

}  // namespace

void WalletsApp::onStart(AppContext& ctx) {
    // The wallet stack is a shared system service (WalletSystem, registered at boot) —
    // the same wallet custom apps reach via nema.wallet.*. Resolve the controller.
    ctl_ = ctx.runtime().container().resolve<wallet::WalletController>();

    kb_.linear = !ctx.runtime().capabilities().has("input.2d");
    state_ = State::Home;        // always land on the menu, never jump into a flow
    suppressActivate_ = true;    // ignore the launcher's Activate that opened us
}

void WalletsApp::refreshAccounts(uint32_t accountIndex) {
    acctIdx_ = accountIndex;
    accounts_ = ctl_->accounts(WalletController::defaultNetworks(), accountIndex);
    rowCtx_.clear();
    rowCtx_.reserve(accounts_.size());
    for (int i = 0; i < (int)accounts_.size(); i++) rowCtx_.push_back({this, i});
}

// ── onPress trampolines ──────────────────────────────────────────────────────
void WalletsApp::cbCreate(void* u) {
    auto* self = static_cast<WalletsApp*>(u);
    if (self->swallowFirst()) return;
    std::string m;
    if (!WalletController::generateMnemonic(false, m)) { self->errorMsg_ = "RNG error"; return; }
    self->revealMnemonic_ = m;
    std::memset(self->wordBuf_, 0, sizeof(self->wordBuf_));
    int i = 0;
    size_t pos = 0;
    while (pos < m.size() && i < 24) {
        size_t sp = m.find(' ', pos);
        std::string w = m.substr(pos, sp == std::string::npos ? std::string::npos : sp - pos);
        std::snprintf(self->wordBuf_[i], sizeof(self->wordBuf_[i]), "%d. %s", i + 1, w.c_str());
        i++;
        if (sp == std::string::npos) break;
        pos = sp + 1;
    }
    self->state_ = State::Reveal;
}

void WalletsApp::cbRestore(void* u) {
    auto* self = static_cast<WalletsApp*>(u);
    if (self->swallowFirst()) return;
    self->kb_.clear();
    self->kb_.setPassword(false);
    self->pendingMnemonic_.clear();
    self->state_ = State::Restore;
}

void WalletsApp::cbRevealContinue(void* u) {
    auto* self = static_cast<WalletsApp*>(u);
    self->pendingMnemonic_ = self->revealMnemonic_;
    if (self->ctl_->state() == wallet::WalletState::Unlocked) {
        // Adding an extra wallet — vault already unlocked, no PIN prompt.
        self->ctl_->addWallet(self->pendingMnemonic_);
        self->pendingMnemonic_.clear();
        self->state_ = State::WalletList;
    } else {
        // First wallet — set the vault PIN next.
        self->kb_.clear();
        self->kb_.setPassword(true);
        self->state_ = State::SetPin;
    }
}

void WalletsApp::cbOpenSettings(void* u) {
    auto* self = static_cast<WalletsApp*>(u);
    if (self->swallowFirst()) return;
    self->state_ = State::Settings;
}

void WalletsApp::cbAbout(void* u) {
    auto* self = static_cast<WalletsApp*>(u);
    if (self->swallowFirst()) return;
    self->state_ = State::About;
}

void WalletsApp::cbMyWallets(void* u) {
    auto* self = static_cast<WalletsApp*>(u);
    if (self->swallowFirst()) return;
    if (self->ctl_->state() == wallet::WalletState::Unlocked) {
        self->state_ = State::WalletList;
    } else {                       // Locked → prompt for PIN, then show the list
        self->kb_.clear();
        self->kb_.setPassword(true);
        self->state_ = State::Locked;
    }
}

void WalletsApp::cbAddChoose(void* u) {
    auto* self = static_cast<WalletsApp*>(u);
    self->state_ = State::AddChoose;
}

void WalletsApp::cbSelectWallet(void* u) {
    auto* rc = static_cast<RowCtx*>(u);
    const auto& wl = rc->self->ctl_->wallets();
    if (rc->idx >= 0 && rc->idx < (int)wl.size()) {
        rc->self->ctl_->selectWallet(wl[rc->idx].id);
        rc->self->state_ = State::Accounts;   // account list of the chosen wallet
    }
}

void WalletsApp::cbAddAccount(void* u) {
    auto* self = static_cast<WalletsApp*>(u);
    self->ctl_->addAccount();   // stays on the account list (now shows one more)
}

void WalletsApp::cbOpenAccountIdx(void* u) {
    auto* rc = static_cast<RowCtx*>(u);
    auto* self = rc->self;
    self->refreshAccounts(static_cast<uint32_t>(rc->idx));   // derive that account's addresses
    self->acctTitle_ = "Account " + std::to_string(rc->idx + 1);
    self->state_ = State::AcctDetail;
}

void WalletsApp::cbShowKey(void* u) {
    auto* self = static_cast<WalletsApp*>(u);
    // Key export is for the chain currently being viewed (the Receive screen).
    if (self->selected_ >= 0 && self->selected_ < (int)self->accounts_.size()) {
        self->exportNet_ = self->accounts_[self->selected_].networkId;
        self->exportTitle_ = "Private key — " + self->accounts_[self->selected_].label;
    }
    self->wantExport_ = true;
}

void WalletsApp::cbExportConfirm(void* u) {
    auto* self = static_cast<WalletsApp*>(u);
    self->wantExport_ = false;
    std::string hex;
    if (!self->ctl_->exportPrivateKey(self->acctIdx_, self->exportNet_.c_str(), hex)) return;
    // Split into 16-char rows for a readable, scrollable reveal.
    self->exportLines_.clear();
    for (size_t i = 0; i < hex.size(); i += 16) self->exportLines_.push_back(hex.substr(i, 16));
    self->state_ = State::ExportKey;
}

void WalletsApp::cbExportCancel(void* u) { static_cast<WalletsApp*>(u)->wantExport_ = false; }

void WalletsApp::cbOpenAccount(void* u) {
    auto* rc = static_cast<RowCtx*>(u);
    rc->self->selected_ = rc->idx;
    rc->self->state_ = State::Receive;
}

void WalletsApp::cbWipeConfirm(void* u) {
    auto* self = static_cast<WalletsApp*>(u);
    self->ctl_->wipe();
    self->wantWipe_ = false;
    self->accounts_.clear();
    self->state_ = State::Home;
}

void WalletsApp::cbWipeCancel(void* u) { static_cast<WalletsApp*>(u)->wantWipe_ = false; }

// ── input ────────────────────────────────────────────────────────────────────
bool WalletsApp::capturesInput() const {
    return state_ == State::Restore || state_ == State::SetPin || state_ == State::Locked;
}

bool WalletsApp::drawRaw(Canvas& c, AppContext&) {
    if (!capturesInput()) return false;
    const char* prompt = state_ == State::Restore ? "Enter recovery phrase"
                       : state_ == State::SetPin  ? "Set a PIN"
                                                  : "Enter PIN";
    kb_.draw(c, prompt);
    return true;
}

bool WalletsApp::onKey(Key k, AppContext& /*ctx*/) {
    // Keyboard states own all input.
    if (capturesInput()) {
        bool done = false, cancel = false;
        kb_.handle(k, done, cancel);
        if (cancel) { state_ = State::Home; return true; }  // back to the menu
        if (done) {
            std::string entry(kb_.buf, kb_.len);
            if (state_ == State::Restore) {
                if (!WalletController::validateMnemonic(entry)) {
                    errorMsg_ = "Invalid phrase";
                    kb_.clear();
                } else if (ctl_->state() == wallet::WalletState::Unlocked) {
                    ctl_->addWallet(entry);            // extra wallet, vault already unlocked
                    kb_.clear();
                    state_ = State::WalletList;
                } else {
                    pendingMnemonic_ = entry;          // first wallet → set PIN next
                    kb_.clear(); kb_.setPassword(true);
                    state_ = State::SetPin;
                }
            } else if (state_ == State::SetPin) {
                if (ctl_->createFirstWallet(pendingMnemonic_, entry)) {
                    pendingMnemonic_.clear();
                    state_ = State::Accounts;   // account list of the new wallet
                } else {
                    errorMsg_ = "Could not create wallet";
                    state_ = State::Home;
                }
                kb_.clear();
            } else {  // Locked
                if (ctl_->unlock(entry)) state_ = State::WalletList;
                else errorMsg_ = "Wrong PIN";
                kb_.clear();
            }
        }
        return true;
    }

    if (k == Key::Cancel) {
        switch (state_) {
            case State::Receive:    state_ = State::AcctDetail; return true;
            case State::ExportKey:  state_ = State::Receive;    return true;
            case State::AcctDetail: state_ = State::Accounts;   return true;
            case State::Accounts:   state_ = State::WalletList; return true;
            case State::AddChoose:  state_ = State::WalletList; return true;
            case State::WalletList:
            case State::Settings:
            case State::About:
            case State::Reveal:     state_ = State::Home;       return true;
            default: return false;  // Home → let the base exit the app
        }
    }
    return false;
}

// ── views ──────────────────────────────────────────────────────────────────
UiNode* WalletsApp::build(NodeArena& arena, AppContext&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    auto row = [&](const char* label, const char* value, void (*onPress)(void*), void* user,
                   bool chevron) {
        ListEntry e;
        e.label = label; e.value = value; e.onPress = onPress; e.user = user; e.chevron = chevron;
        return ListItemRow(arena, e);
    };

    if (capturesInput())  // keyboard paints via drawRaw; tree unused
        return View(arena, root, {});

    if (!ctl_) {
        return View(arena, root, {ListContainer(arena, scroll_, {
            ListSection(arena, "Wallet"),
            Text(arena, "Wallet system unavailable", TextRole::Body),
        })});
    }

    if (state_ == State::Home) {
        const bool has = ctl_->state() != wallet::WalletState::NoWallet;
        UiNode* lc = ListContainer(arena, scroll_, {});
        std::vector<UiNode*> kids;
        kids.push_back(ListSection(arena, "Wallets"));
        if (has) {
            kids.push_back(row("My Wallets", nullptr, cbMyWallets, this, true));
            kids.push_back(row("Settings", nullptr, cbOpenSettings, this, true));
        } else {
            kids.push_back(row("Create new wallet", nullptr, cbCreate, this, true));
            kids.push_back(row("Restore from phrase", nullptr, cbRestore, this, true));
        }
        kids.push_back(row("About", nullptr, cbAbout, this, true));
        linkChildren(lc, kids);
        return View(arena, root, {lc});
    }

    if (state_ == State::WalletList) {
        UiNode* lc = ListContainer(arena, scroll_, {});
        std::vector<UiNode*> kids;
        const auto& wl = ctl_->wallets();
        walletRowCtx_.clear();
        walletRowCtx_.reserve(wl.size());
        for (int i = 0; i < (int)wl.size(); i++) walletRowCtx_.push_back({this, i});
        kids.push_back(ListSection(arena, "Wallets"));
        for (int i = 0; i < (int)wl.size(); i++) {
            const char* val = wl[i].id == ctl_->activeId() ? "active" : nullptr;
            kids.push_back(row(wl[i].label.c_str(), val, cbSelectWallet, &walletRowCtx_[i], true));
        }
        kids.push_back(ListSection(arena, "Add"));
        kids.push_back(row("Add wallet", nullptr, cbAddChoose, this, true));
        linkChildren(lc, kids);
        return View(arena, root, {lc});
    }

    if (state_ == State::AddChoose) {
        return View(arena, root, {
            ListContainer(arena, scroll_, {
                ListSection(arena, "Add wallet"),
                row("Create new wallet", nullptr, cbCreate, this, true),
                row("Import recovery phrase", nullptr, cbRestore, this, true),
            }),
        });
    }

    if (state_ == State::Reveal) {
        UiNode* lc = ListContainer(arena, scroll_, {});
        std::vector<UiNode*> kids;
        kids.push_back(ListSection(arena, "Write these down"));
        // Words are focusable-but-no-op so Up/Down can scroll through the whole phrase
        // (ListContainer scrolls to follow focus; non-focusable rows can't be scrolled to).
        for (int i = 0; i < 24 && wordBuf_[i][0]; i++)
            kids.push_back(row(wordBuf_[i], nullptr, [](void*) {}, this, false));
        kids.push_back(row("Continue", nullptr, cbRevealContinue, this, true));
        linkChildren(lc, kids);
        return View(arena, root, {lc});
    }

    // Account LIST (one seed → many accounts, BIP44 index).
    if (state_ == State::Accounts) {
        uint32_t n = ctl_->accountCount();
        acctLabels_.clear();
        acctRowCtx_.clear();
        for (uint32_t i = 0; i < n; i++) {
            acctLabels_.push_back("Account " + std::to_string(i + 1));
            acctRowCtx_.push_back({this, (int)i});
        }
        UiNode* lc = ListContainer(arena, scroll_, {});
        std::vector<UiNode*> kids;
        kids.push_back(ListSection(arena, "Accounts"));
        for (uint32_t i = 0; i < n; i++)
            kids.push_back(row(acctLabels_[i].c_str(), nullptr, cbOpenAccountIdx, &acctRowCtx_[i], true));
        kids.push_back(ListSection(arena, "Add"));
        kids.push_back(row("Add account", nullptr, cbAddAccount, this, true));
        linkChildren(lc, kids);
        return View(arena, root, {lc});
    }

    // One account's addresses per chain + key export.
    if (state_ == State::AcctDetail) {
        const char* badge = ctl_->backendKind() == wallet::BackendKind::SecureElement
                                ? "Secure Element" : "Software key";
        UiNode* lc = ListContainer(arena, scroll_, {});
        std::vector<UiNode*> kids;
        rowVal_.clear();
        rowVal_.reserve(accounts_.size());
        for (int i = 0; i < (int)accounts_.size(); i++) rowVal_.push_back(truncAddr(accounts_[i].address));
        kids.push_back(ListSection(arena, acctTitle_.c_str()));   // "Account N" as the subheader
        for (int i = 0; i < (int)accounts_.size(); i++)
            kids.push_back(row(accounts_[i].label.c_str(), rowVal_[i].c_str(), cbOpenAccount, &rowCtx_[i], true));
        kids.push_back(ListSection(arena, "Wallet"));
        kids.push_back(row("Security", badge, [](void*) {}, this, false));
        linkChildren(lc, kids);
        return View(arena, root, {lc});
    }

    if (state_ == State::ExportKey) {
        UiNode* lc = ListContainer(arena, scroll_, {});
        std::vector<UiNode*> kids;
        kids.push_back(ListSection(arena, exportTitle_.c_str()));   // "Private key — Ethereum"
        for (auto& ln : exportLines_) kids.push_back(row(ln.c_str(), nullptr, [](void*) {}, this, false));
        kids.push_back(row("Anyone with this key owns the account", nullptr, [](void*) {}, this, false));
        linkChildren(lc, kids);
        return View(arena, root, {lc});
    }

    if (state_ == State::About) {
        const char* badge = ctl_->backendKind() == wallet::BackendKind::SecureElement
                                ? "Secure Element" : "Software key";
        return View(arena, root, {
            ListContainer(arena, scroll_, {
                ListSection(arena, "About"),
                row("Wallet", "v1.0.0", [](void*) {}, this, false),
                row("Security", badge, [](void*) {}, this, false),
                row("Build", "Dev \xc2\xb7 testnet only", [](void*) {}, this, false),
                row("Chains", "BTC \xc2\xb7 EVM \xc2\xb7 Solana", [](void*) {}, this, false),
            }),
        });
    }

    if (state_ == State::Receive) {
        const auto& a = accounts_[selected_];
        return View(arena, root, {
            ListContainer(arena, scroll_, {
                ListSection(arena, a.label.c_str()),   // "Ethereum" / "Bitcoin" / "Solana"
                Text(arena, a.address.c_str(), TextRole::Body),
                ListSection(arena, "Keys"),
                row("Show private key", nullptr, cbShowKey, this, true),
            }),
        });
    }

    // Settings
    return View(arena, root, {
        ListContainer(arena, scroll_, {
            ListSection(arena, "Settings"),
            row("Wipe wallet", nullptr, [](void* u) { static_cast<WalletsApp*>(u)->wantWipe_ = true; },
                this, true),
        }),
    });
}

UiNode* WalletsApp::buildModal(NodeArena& arena, AppContext&) {
    if (wantWipe_) {
        wipeBtns_[0] = {"Wipe", cbWipeConfirm, this, false};
        wipeBtns_[1] = {"Cancel", cbWipeCancel, this, false};
        return Dialog(arena, "Wipe wallet?", "Erases this wallet.\nBack up your phrase!",
                      nullptr, 0, 0, wipeBtns_, 2);
    }
    if (wantExport_) {
        wipeBtns_[0] = {"Reveal", cbExportConfirm, this, false};
        wipeBtns_[1] = {"Cancel", cbExportCancel, this, false};
        return Dialog(arena, "Show private key?", "Anyone with it owns\nthis account.",
                      nullptr, 0, 0, wipeBtns_, 2);
    }
    return nullptr;
}

}  // namespace nema
