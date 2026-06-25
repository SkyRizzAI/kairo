#include "nema/wallet/wallet_service.h"
#include "nema/wallet/network_registry.h"

namespace nema::wallet {

IChain* WalletService::chainForNetwork(const NetworkParams& net) const {
    auto it = chains_.find(net.family);
    return it == chains_.end() ? nullptr : it->second;
}

bool WalletService::deriveAddress(const char* networkId, uint32_t index, std::string& out) {
    if (!backend_.ready()) return false;
    const NetworkParams* net = NetworkRegistry::find(networkId);
    if (!net) return false;
    IChain* chain = chainForNetwork(*net);
    if (!chain) return false;
    PubKey pub;
    if (!backend_.publicKey(chain->pathFor(index, *net), chain->info().curve, pub)) return false;
    out = chain->addressFromPubkey(pub, *net);
    return !out.empty();
}

bool WalletService::previewTransaction(const char* networkId, const Bytes& rawTx, TxPreview& out) {
    const NetworkParams* net = NetworkRegistry::find(networkId);
    if (!net) return false;
    IChain* chain = chainForNetwork(*net);
    if (!chain) return false;
    out = chain->decodeForDisplay(rawTx, *net);
    return true;
}

bool WalletService::signTransaction(const char* networkId, uint32_t index, const Bytes& rawTx,
                                    const Confirm& confirm, Bytes& signedOut) {
    if (!backend_.ready()) return false;
    const NetworkParams* net = NetworkRegistry::find(networkId);
    if (!net) return false;
    IChain* chain = chainForNetwork(*net);
    if (!chain) return false;

    // WYSIWYS consent — decode the EXACT bytes we will sign, then ask (decode == sign).
    TxPreview preview = chain->decodeForDisplay(rawTx, *net);
    if (!confirm || !confirm(preview)) return false;  // fail-closed (reject / no gate)

    DerivationPath account = chain->pathFor(index, *net);
    std::vector<SigningItem> items = chain->buildSigningPayload(rawTx, *net, account);
    if (items.empty()) return false;

    std::vector<Signature> sigs;
    sigs.reserve(items.size());
    for (const auto& it : items) {
        Signature sig;
        if (!backend_.sign(it.path, it.curve, it.payload.data(), it.payload.size(), it.prehashed, sig))
            return false;
        sigs.push_back(std::move(sig));
    }
    signedOut = chain->encodeSigned(rawTx, sigs, *net);
    return !signedOut.empty();
}

bool WalletService::signMessage(const char* networkId, uint32_t index, const Bytes& msg,
                                const Confirm& confirm, Signature& sigOut) {
    if (!backend_.ready()) return false;
    const NetworkParams* net = NetworkRegistry::find(networkId);
    if (!net) return false;
    IChain* chain = chainForNetwork(*net);
    if (!chain) return false;

    SigningItem item = chain->messageSigningItem(msg, chain->pathFor(index, *net));
    if (item.payload.empty()) return false;  // chain doesn't support message signing

    TxPreview preview;
    MsgPreview mp = chain->decodeMessage(msg, MsgKind::Personal);
    preview.rows.push_back({"Sign message", mp.text});
    if (!confirm || !confirm(preview)) return false;  // fail-closed

    return backend_.sign(item.path, item.curve, item.payload.data(), item.payload.size(),
                         item.prehashed, sigOut);
}

}  // namespace nema::wallet
