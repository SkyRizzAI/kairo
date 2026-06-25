#include "nema/wallet/wallet_controller.h"
#include "nema/wallet/network_registry.h"

namespace nema::wallet {

std::vector<WalletController::AccountView> WalletController::accounts(
    const std::vector<std::string>& networkIds, uint32_t index) {
    std::vector<AccountView> out;
    if (state() != WalletState::Unlocked) return out;
    for (const auto& id : networkIds) {
        const NetworkParams* net = NetworkRegistry::find(id.c_str());
        std::string addr;
        if (!svc_.deriveAddress(id.c_str(), index, addr)) continue;
        out.push_back({id, net ? net->label : id, addr});
    }
    return out;
}

bool WalletController::exportPrivateKey(uint32_t accountIndex, const char* networkId,
                                        std::string& hexOut) {
    if (state() != WalletState::Unlocked) return false;
    const NetworkParams* net = NetworkRegistry::find(networkId);
    if (!net) return false;
    IChain* chain = svc_.chainForNetwork(*net);
    if (!chain) return false;
    Bytes raw;
    if (!vault_.exportPrivateKey(chain->pathFor(accountIndex, *net), chain->info().curve, raw))
        return false;
    static const char* h = "0123456789abcdef";
    hexOut.clear();
    for (uint8_t b : raw) { hexOut += h[b >> 4]; hexOut += h[b & 0xf]; }
    return true;
}

}  // namespace nema::wallet
