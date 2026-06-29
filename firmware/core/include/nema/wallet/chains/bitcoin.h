#pragma once
#include "nema/wallet/chain.h"

// BitcoinChain — UTXO family (Plan 94, ADR 0015). secp256k1, BIP84 native SegWit
// (P2WPKH, bech32 addresses).
//
// SCOPE: address derivation + pathFor (receive path) and native-SegWit P2WPKH
// transaction signing are implemented and tested. The signer uses BIP143 sighashing
// over a compact "Palanu BTC v1" sign-request (see bitcoin.cpp) instead of full PSBT,
// and is verified against the official BIP143 P2WPKH test vector in
// firmware/tests/bitcoin_chain_test.cpp (a wrong sighash = lost funds). Message
// signing uses the legacy "Bitcoin Signed Message" format. NOT yet covered: non-segwit
// (P2PKH) and nested/taproot inputs, multi-key/PSBT, and BIP-322 — these fail closed
// (decodeForDisplay → blind-sign, buildSigningPayload → empty), never signing blind.

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
