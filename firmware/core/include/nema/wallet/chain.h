#pragma once
#include "nema/wallet/wallet_types.h"

// IChain — CHAIN axis (Plan 94, ADR 0015). Protocol-level, custody-blind.

namespace nema::wallet {

// One implementation per protocol FAMILY (EvmChain / BitcoinChain / SolanaChain),
// NOT per network. EVM networks (Ethereum, BNB, Polygon, Arbitrum, ...) share
// EvmChain and differ only by NetworkParams::chainId — networks are data
// (see NetworkRegistry), not code.
//
// A chain driver knows address format, serialization, what bytes to hash, and how
// to decode a tx for display — but knows nothing about where the key lives. It
// delegates the actual signing to an IWalletBackend.
struct IChain {
    virtual ~IChain() = default;

    virtual ChainInfo info() const = 0;

    // Address string from a public key, for the given network.
    virtual std::string addressFromPubkey(const PubKey& pub, const NetworkParams& net) const = 0;

    // BIP44 derivation path for account `index` on `net`.
    virtual DerivationPath pathFor(uint32_t index, const NetworkParams& net) const = 0;

    // Decode a raw tx into human-readable rows for the consent modal (WYSIWYS).
    // Sets TxPreview::blindSign when it cannot fully decode (caller shows a warning).
    virtual TxPreview decodeForDisplay(const Bytes& rawTx, const NetworkParams& net) const = 0;

    // What must be signed. May be >1 item (BTC PSBT = one per input; EVM/SVM = one).
    // `account` is the signing account's derivation path; EVM/SVM apply it to the single
    // item, BTC reads per-input paths from the PSBT (using `account` to validate ownership).
    // The bytes summarised by decodeForDisplay MUST correspond to these (decode == sign).
    virtual std::vector<SigningItem> buildSigningPayload(const Bytes& rawTx,
                                                         const NetworkParams& net,
                                                         const DerivationPath& account) const = 0;

    // Assemble the final signed tx from the signatures (order matches buildSigningPayload).
    virtual Bytes encodeSigned(const Bytes& rawTx, const std::vector<Signature>& sigs,
                               const NetworkParams& net) const = 0;

    // Decode a message (personal_sign / EIP-712 / Solana message) for display.
    virtual MsgPreview decodeMessage(const Bytes& msg, MsgKind kind) const = 0;

    // Build the signing item for a personal message at `path` (EVM: EIP-191 prefix +
    // keccak digest; Solana: the raw message under Ed25519). Returns an item with an
    // empty payload if message signing isn't supported for this chain.
    virtual SigningItem messageSigningItem(const Bytes& msg, const DerivationPath& path) const = 0;
};

} // namespace nema::wallet
