#include "nema/wallet/chains/bitcoin.h"

#include <cstring>

extern "C" {
#include "hasher.h"
#include "segwit_addr.h"
}

namespace nema::wallet {

ChainInfo BitcoinChain::info() const { return {"bitcoin", 0, Curve::Secp256k1}; }

DerivationPath BitcoinChain::pathFor(uint32_t index, const NetworkParams&) const {
    constexpr uint32_t H = DerivationPath::Hardened;
    DerivationPath p;
    p.indices = {84u | H, 0u | H, 0u | H, 0u, index};  // BIP84 m/84'/0'/0'/0/index (P2WPKH)
    return p;
}

std::string BitcoinChain::addressFromPubkey(const PubKey& pub, const NetworkParams& net) const {
    if (pub.size() != 33) return "";  // P2WPKH uses the compressed pubkey
    uint8_t h160[32];
    hasher_Raw(HASHER_SHA2_RIPEMD, pub.data(), 33, h160);  // hash160 = ripemd160(sha256(pub))
    const char* hrp = net.testnet ? "tb" : "bc";
    char out[100];
    if (segwit_addr_encode(out, hrp, 0, h160, 20) != 1) return "";
    return std::string(out);
}

// ── Transaction signing: BIP143 + PSBT — focused follow-up (see header). Fail closed. ──

TxPreview BitcoinChain::decodeForDisplay(const Bytes&, const NetworkParams& net) const {
    TxPreview p;
    p.rows.push_back({"Network", net.label});
    p.blindSign = true;
    p.warning = "Bitcoin transaction decoding not yet implemented";
    return p;
}

std::vector<SigningItem> BitcoinChain::buildSigningPayload(const Bytes&, const NetworkParams&,
                                                           const DerivationPath&) const {
    return {};  // BIP143 sighash TODO — never sign unverified
}

Bytes BitcoinChain::encodeSigned(const Bytes&, const std::vector<Signature>&,
                                 const NetworkParams&) const {
    return {};
}

SigningItem BitcoinChain::messageSigningItem(const Bytes&, const DerivationPath& path) const {
    // BIP-322 message signing is a follow-up — empty payload signals "unsupported".
    SigningItem it;
    it.path = path;
    it.curve = Curve::Secp256k1;
    it.prehashed = true;
    return it;
}

MsgPreview BitcoinChain::decodeMessage(const Bytes& msg, MsgKind) const {
    MsgPreview p;
    p.text.assign(msg.begin(), msg.end());  // BIP-322 / signmessage text
    return p;
}

}  // namespace nema::wallet
