#pragma once
#include "nema/wallet/wallet_vault.h"
#include "nema/wallet/wallet_service.h"
#include "nema/wallet/wallet_consent_service.h"
#include "nema/wallet/wallet_controller.h"
#include "nema/wallet/chains/evm.h"
#include "nema/wallet/chains/solana.h"
#include "nema/wallet/chains/bitcoin.h"

// WalletSystem — the single, shared wallet stack (Plan 94, Fase 7). Registered in the
// runtime container at boot so BOTH the Wallets app and custom apps (via nema.wallet.*)
// use the SAME wallet. Owns one vault (multiple wallets, one PIN), the chain drivers,
// the service, the consent plumbing, and the controller.

namespace nema {
class Runtime;
}

namespace nema::wallet {

class WalletSystem {
public:
    // se = device-bound sealing chip selected by bootWalletSystem (nullptr → software).
    explicit WalletSystem(IKvStore& store, ISecureElement* se = nullptr);

    // Register WalletService / WalletConsentService / WalletController into the container.
    void registerInto(Runtime& rt);

    WalletService&        service()    { return svc_; }
    WalletConsentService& consent()    { return consent_; }
    WalletController&     controller() { return ctl_; }

private:
    WalletVault          vault_;
    EvmChain             evm_;
    SolanaChain          sol_;
    BitcoinChain         btc_;
    WalletService        svc_;
    WalletConsentService consent_;
    WalletController     ctl_;
};

// Construct (once) the shared wallet stack backed by internal-flash storage and register
// it into the container. Call from target main() after rt.start(). Returns the singleton.
WalletSystem& bootWalletSystem(Runtime& rt);

}  // namespace nema::wallet
