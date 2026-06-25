#pragma once
#include "nema/wallet/chain.h"

// EvmChain — one driver for ALL EVM networks (Plan 94, ADR 0015). Ethereum, BNB,
// Polygon, Arbitrum, … differ only by NetworkParams::chainId. secp256k1 + keccak256.
//
// rawTx convention (what the bridge/app hands in):
//   legacy / EIP-155 : rlp([nonce,gasPrice,gas,to,value,data,chainId,"",""])   (a list)
//   EIP-1559 (type 2): 0x02 || rlp([chainId,nonce,maxPrio,maxFee,gas,to,value,data,accessList])
// These ARE the signing preimages; buildSigningPayload returns keccak256(rawTx).

namespace nema::wallet {

class EvmChain : public IChain {
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

}  // namespace nema::wallet
