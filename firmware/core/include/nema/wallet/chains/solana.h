#pragma once
#include "nema/wallet/chain.h"

// SolanaChain — SVM family (Plan 94, ADR 0015). Ed25519 / SLIP-0010, base58 addresses.
//
// rawTx convention: the serialized (legacy) Solana **message** — exactly the bytes that
// get signed. buildSigningPayload returns the whole message (Ed25519 signs it directly,
// prehashed=false); encodeSigned wraps it into the wire transaction
// (compact-u16 sigCount || signatures || message).

namespace nema::wallet {

class SolanaChain : public IChain {
public:
    ChainInfo info() const override;
    std::string addressFromPubkey(const PubKey& pub, const NetworkParams& net) const override;
    DerivationPath pathFor(uint32_t index, const NetworkParams& net) const override;
    TxPreview decodeForDisplay(const Bytes& rawTx, const NetworkParams& net) const override;
    std::vector<SigningItem> buildSigningPayload(const Bytes& rawTx, const NetworkParams& net,
                                                 const DerivationPath& account) const override;
    Bytes encodeSigned(const Bytes& rawTx, const std::vector<Signature>& sigs,
                       const NetworkParams& net) const override;
    MsgPreview decodeMessage(const Bytes& msg, MsgKind kind) const override;
    SigningItem messageSigningItem(const Bytes& msg, const DerivationPath& path) const override;
};

// base58 (raw, no checksum) — public for reuse + testing.
std::string base58Encode(const uint8_t* data, size_t len);

}  // namespace nema::wallet
