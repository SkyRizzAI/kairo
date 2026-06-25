#pragma once
#include "nema/wallet/wallet_backend.h"
#include "nema/wallet/chain.h"
#include "nema/wallet/wallet_types.h"

#include <functional>
#include <string>
#include <unordered_map>

// WalletService — orchestrator across the three axes (Plan 94, ADR 0015):
// custody (IWalletBackend) × chain (IChain, by family) × consent (a confirm callback).
// All consumers (native app / on-device JS / dApp bridge) funnel through this one
// service and one consent gate — one security path, not three.

namespace nema::wallet {

class WalletService {
public:
    // confirm(preview) is the consent gate — return true to proceed. Fase 5 wires this
    // to the system trusted-display modal + physical button; tests inject auto-approve.
    using Confirm = std::function<bool(const TxPreview&)>;

    explicit WalletService(IWalletBackend& backend) : backend_(backend) {}

    void registerChain(IChain& chain) { chains_[chain.info().family] = &chain; }

    IWalletBackend& backend() { return backend_; }
    BackendKind activeBackendKind() const { return backend_.kind(); }  // → UI indicator
    bool ready() const { return backend_.ready(); }
    IChain* chainForNetwork(const NetworkParams& net) const;

    // Derive the address for (network id, BIP44 account index).
    bool deriveAddress(const char* networkId, uint32_t index, std::string& out);

    // Decode a raw tx for display (WYSIWYS) without signing.
    bool previewTransaction(const char* networkId, const Bytes& rawTx, TxPreview& out);

    // Sign: decode → confirm(preview) → backend.sign(each item) → chain.encodeSigned.
    // Fail-closed: false if locked, unknown network, chain can't build payload, or rejected.
    bool signTransaction(const char* networkId, uint32_t index, const Bytes& rawTx,
                         const Confirm& confirm, Bytes& signedOut);

    // Sign a personal message (same consent gate). Returns the raw signature. Fails if
    // the chain doesn't support message signing.
    bool signMessage(const char* networkId, uint32_t index, const Bytes& msg,
                     const Confirm& confirm, Signature& sigOut);

private:
    IWalletBackend& backend_;
    std::unordered_map<std::string, IChain*> chains_;
};

}  // namespace nema::wallet
