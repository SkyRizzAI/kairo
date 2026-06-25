#pragma once
#include "nema/wallet/wallet_types.h"

// NetworkRegistry — the DATA axis behind IChain (Plan 94, ADR 0015).

namespace nema::wallet {

// Networks are data, not code: adding an EVM network (zkSync, Linea, ...) is a new
// entry here, with NO chain-driver change. MVP: an embedded constexpr table
// (trusted, no injection). User-supplied networks, when added later, must be
// validated before use.
//
// Implementation lands in Fase 2 (network_registry.cpp) — this is the contract.
class NetworkRegistry {
public:
    // Look up a network by id (e.g. "eth-mainnet"). Returns nullptr if unknown.
    static const NetworkParams* find(const char* id);

    // All built-in networks (for the UI network picker).
    static const std::vector<NetworkParams>& all();
};

} // namespace nema::wallet
