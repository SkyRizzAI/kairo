#include "nema/wallet/network_registry.h"

#include <cstring>

// Built-in network table (Plan 94, ADR 0015 — networks are DATA). EVM entries all map
// to the one EvmChain and differ only by chainId; adding an EVM L2 is a new row here
// with no chain-driver change.

namespace nema::wallet {

const std::vector<NetworkParams>& NetworkRegistry::all() {
    static const std::vector<NetworkParams> nets = {
        // id              family     label               chainId    testnet
        {"eth-mainnet",   "evm",     "Ethereum",         1,         false},
        {"bnb",           "evm",     "BNB Smart Chain",  56,        false},
        {"polygon",       "evm",     "Polygon",          137,       false},
        {"arbitrum",      "evm",     "Arbitrum One",     42161,     false},
        {"optimism",      "evm",     "Optimism",         10,        false},
        {"base",          "evm",     "Base",             8453,      false},
        {"avalanche",     "evm",     "Avalanche C",      43114,     false},
        {"sepolia",       "evm",     "Sepolia",          11155111,  true},
        {"btc-mainnet",   "bitcoin", "Bitcoin",          0,         false},
        {"btc-testnet",   "bitcoin", "Bitcoin Testnet",  0,         true},
        {"sol-mainnet",   "solana",  "Solana",           0,         false},
        {"sol-devnet",    "solana",  "Solana Devnet",    0,         true},
    };
    return nets;
}

const NetworkParams* NetworkRegistry::find(const char* id) {
    if (!id) return nullptr;
    for (const auto& n : all())
        if (std::strcmp(n.id, id) == 0) return &n;
    return nullptr;
}

}  // namespace nema::wallet
