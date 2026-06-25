#pragma once
#include "nema/wallet/chain.h"

// BitcoinChain — UTXO family (Plan 94, ADR 0015). secp256k1, BIP84 native SegWit
// (P2WPKH, bech32 addresses).
//
// SCOPE (Fase 2): address derivation + pathFor are complete and tested (the
// app-critical "receive" path). Transaction decode/sign/encode require BIP143 segwit
// sighashing and PSBT parsing — these MUST be verified against the official BIP143 test
// vector before signing real value (a wrong sighash = lost funds), so they are
// deliberately left as a focused follow-up and currently fail closed (blind-sign /
// empty). BTC *sending* is not needed for initial app bring-up; receiving is.

namespace nema::wallet {

class BitcoinChain : public IChain {
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
