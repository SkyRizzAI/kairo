#include "nema/wallet/chains/evm.h"
#include "nema/wallet/chains/rlp.h"

#include <cstring>

extern "C" {
#include "sha3.h"
#include "ecdsa.h"
#include "secp256k1.h"
}

namespace nema::wallet {

namespace {

const char* kHex = "0123456789abcdef";

std::string toHex(const uint8_t* p, size_t n, bool prefix = true) {
    std::string s = prefix ? "0x" : "";
    for (size_t i = 0; i < n; i++) { s += kHex[p[i] >> 4]; s += kHex[p[i] & 0xf]; }
    return s;
}

Bytes stripLeadingZeros(const Bytes& b) {
    size_t s = 0;
    while (s < b.size() && b[s] == 0) s++;
    return Bytes(b.begin() + s, b.end());
}

uint64_t beToU64(const Bytes& b) {
    uint64_t v = 0;
    for (uint8_t x : b) v = (v << 8) | x;
    return v;
}

Bytes u64ToBE(uint64_t v) {
    Bytes out;
    while (v) { out.insert(out.begin(), static_cast<uint8_t>(v & 0xff)); v >>= 8; }
    return out;
}

// Decimal string of a big-endian unsigned integer (schoolbook divmod-10; n ≤ 32).
std::string bytesToDec(Bytes n) {
    n = stripLeadingZeros(n);
    if (n.empty()) return "0";
    std::string out;
    while (!n.empty()) {
        int rem = 0;
        Bytes q;
        for (uint8_t b : n) { int cur = rem * 256 + b; q.push_back(static_cast<uint8_t>(cur / 10)); rem = cur % 10; }
        out.insert(out.begin(), static_cast<char>('0' + rem));
        n = stripLeadingZeros(q);
    }
    return out;
}

// Format an integer decimal string with `decimals` fractional digits, trimming zeros.
std::string formatUnits(std::string dec, int decimals) {
    if (decimals <= 0) return dec;
    while (static_cast<int>(dec.size()) <= decimals) dec.insert(dec.begin(), '0');
    dec.insert(dec.end() - decimals, '.');
    while (dec.back() == '0') dec.pop_back();
    if (dec.back() == '.') dec.pop_back();
    return dec;
}

// Schoolbook multiply of two big-endian unsigned integers (for gas × price).
Bytes bytesMul(const Bytes& a, const Bytes& b) {
    if (a.empty() || b.empty()) return {};
    std::vector<int> r(a.size() + b.size(), 0);
    for (int i = static_cast<int>(a.size()) - 1; i >= 0; i--)
        for (int j = static_cast<int>(b.size()) - 1; j >= 0; j--) {
            int idx = i + j + 1;
            int v = r[idx] + a[i] * b[j];
            r[idx] = v & 0xff;
            r[idx - 1] += v >> 8;
        }
    Bytes out(r.begin(), r.end());
    return stripLeadingZeros(out);
}

void keccak(const Bytes& in, uint8_t out[32]) { keccak_256(in.data(), in.size(), out); }

// Parse the RLP list whose header is at in[start]; return each item's FULL encoding
// (header + content), so list items like the EIP-1559 access-list survive re-encoding.
bool splitRaw(const Bytes& in, size_t start, std::vector<Bytes>& raw) {
    raw.clear();
    if (start >= in.size()) return false;
    uint8_t b = in[start];
    size_t payloadStart, payloadEnd;
    if (b >= 0xf8) {
        size_t ll = b - 0xf7;
        if (start + 1 + ll > in.size()) return false;
        size_t len = 0;
        for (size_t k = 0; k < ll; k++) len = (len << 8) | in[start + 1 + k];
        payloadStart = start + 1 + ll; payloadEnd = payloadStart + len;
    } else if (b >= 0xc0) {
        payloadStart = start + 1; payloadEnd = payloadStart + (b - 0xc0);
    } else {
        return false;
    }
    if (payloadEnd > in.size()) return false;
    size_t p = payloadStart;
    while (p < payloadEnd) {
        uint8_t h = in[p];
        size_t hdr, len;
        if (h <= 0x7f) { hdr = 0; len = 1; }
        else if (h <= 0xb7) { hdr = 1; len = h - 0x80; }
        else if (h <= 0xbf) { size_t ll = h - 0xb7; hdr = 1 + ll; len = 0;
            for (size_t k = 0; k < ll; k++) len = (len << 8) | in[p + 1 + k]; }
        else if (h <= 0xf7) { hdr = 1; len = h - 0xc0; }
        else { size_t ll = h - 0xf7; hdr = 1 + ll; len = 0;
            for (size_t k = 0; k < ll; k++) len = (len << 8) | in[p + 1 + k]; }
        size_t total = hdr + len;
        if (p + total > payloadEnd) return false;
        raw.emplace_back(in.begin() + p, in.begin() + p + total);
        p += total;
    }
    return p == payloadEnd;
}

bool isType2(const Bytes& rawTx) { return !rawTx.empty() && rawTx[0] == 0x02; }

// Decode rawTx into the ordered field CONTENTS (decoded), normalising both layouts to:
//   [0]nonce [1]gasPriceOrMaxFee [2]gas [3]to [4]value [5]data [6]chainId
struct EvmFields { Bytes nonce, price, gas, to, value, data, chainId; bool type2; bool okv = false; };

EvmFields decodeFields(const Bytes& rawTx) {
    EvmFields f;
    f.type2 = isType2(rawTx);
    std::vector<Bytes> it;
    if (f.type2) {
        Bytes inner(rawTx.begin() + 1, rawTx.end());
        if (!rlp::decodeList(inner, it) || it.size() < 9) return f;
        // [chainId,nonce,maxPrio,maxFee,gas,to,value,data,accessList]
        f.chainId = it[0]; f.nonce = it[1]; f.price = it[3]; f.gas = it[4];
        f.to = it[5]; f.value = it[6]; f.data = it[7];
    } else {
        if (!rlp::decodeList(rawTx, it) || it.size() < 7) return f;
        // [nonce,gasPrice,gas,to,value,data,chainId,...]
        f.nonce = it[0]; f.price = it[1]; f.gas = it[2]; f.to = it[3];
        f.value = it[4]; f.data = it[5]; f.chainId = it[6];
    }
    f.okv = true;
    return f;
}

constexpr uint8_t kErc20Transfer[4] = {0xa9, 0x05, 0x9c, 0xbb};  // transfer(address,uint256)

}  // namespace

ChainInfo EvmChain::info() const { return {"evm", 60, Curve::Secp256k1}; }

DerivationPath EvmChain::pathFor(uint32_t index, const NetworkParams&) const {
    constexpr uint32_t H = DerivationPath::Hardened;
    DerivationPath p;
    p.indices = {44u | H, 60u | H, 0u | H, 0u, index};  // m/44'/60'/0'/0/index
    return p;
}

std::string EvmChain::addressFromPubkey(const PubKey& pub, const NetworkParams&) const {
    uint8_t pub65[65];
    if (pub.size() == 33) {
        if (ecdsa_uncompress_pubkey(&secp256k1, pub.data(), pub65) != 1) return "";
    } else if (pub.size() == 65) {
        std::memcpy(pub65, pub.data(), 65);
    } else if (pub.size() == 64) {
        pub65[0] = 0x04; std::memcpy(pub65 + 1, pub.data(), 64);
    } else {
        return "";
    }
    uint8_t h[32];
    keccak_256(pub65 + 1, 64, h);
    return toHex(h + 12, 20);  // lowercase 0x address (EIP-55 checksum is cosmetic)
}

TxPreview EvmChain::decodeForDisplay(const Bytes& rawTx, const NetworkParams& net) const {
    TxPreview p;
    EvmFields f = decodeFields(rawTx);
    if (!f.okv) { p.blindSign = true; p.warning = "Unparseable transaction"; return p; }

    p.rows.push_back({"Network", net.label});

    // ERC-20 transfer(address,uint256) — clear-sign the recipient + base-unit amount.
    if (f.data.size() == 68 && std::memcmp(f.data.data(), kErc20Transfer, 4) == 0) {
        p.rows.push_back({"Type", "ERC-20 transfer"});
        p.rows.push_back({"Token", toHex(f.to.data(), f.to.size())});
        p.rows.push_back({"To", toHex(f.data.data() + 16, 20)});  // arg1: address (right-aligned in 32B)
        Bytes amt(f.data.begin() + 36, f.data.begin() + 68);
        p.rows.push_back({"Amount", bytesToDec(amt) + " (base units)"});
    } else if (f.to.empty()) {
        p.rows.push_back({"To", "Contract creation"});
        p.blindSign = true;
        p.warning = "Deploys contract code";
    } else {
        p.rows.push_back({"To", toHex(f.to.data(), f.to.size())});
        p.rows.push_back({"Amount", formatUnits(bytesToDec(f.value), 18)});
        if (!f.data.empty()) {
            p.blindSign = true;
            p.warning = "Contract call — input data not decoded";
            p.rows.push_back({"Data", toHex(f.data.data(), f.data.size() > 8 ? 8 : f.data.size()) + "…"});
        }
    }

    // Max fee = gas × price (gwei→ETH for display).
    Bytes fee = bytesMul(f.gas, f.price);
    p.rows.push_back({"Max fee", formatUnits(bytesToDec(fee), 18)});
    return p;
}

std::vector<SigningItem> EvmChain::buildSigningPayload(const Bytes& rawTx, const NetworkParams&,
                                                       const DerivationPath& account) const {
    // For both legacy(EIP-155) and type-2, rawTx IS the signing preimage → keccak256.
    uint8_t digest[32];
    keccak(rawTx, digest);
    SigningItem item;
    item.path = account;
    item.curve = Curve::Secp256k1;
    item.payload.assign(digest, digest + 32);
    item.prehashed = true;
    return {item};
}

Bytes EvmChain::encodeSigned(const Bytes& rawTx, const std::vector<Signature>& sigs,
                             const NetworkParams&) const {
    if (sigs.size() != 1 || sigs[0].size() != 65) return {};
    const Signature& sig = sigs[0];
    Bytes r = stripLeadingZeros(Bytes(sig.begin(), sig.begin() + 32));
    Bytes s = stripLeadingZeros(Bytes(sig.begin() + 32, sig.begin() + 64));
    uint8_t recid = sig[64];

    if (isType2(rawTx)) {
        std::vector<Bytes> raw;
        if (!splitRaw(rawTx, 1, raw) || raw.size() < 9) return {};
        std::vector<Bytes> items(raw.begin(), raw.begin() + 9);  // keep all 9 incl. accessList
        items.push_back(rlp::encodeString(Bytes{recid}.front() == 0 ? Bytes{} : Bytes{recid}));  // v=0→empty
        items.push_back(rlp::encodeString(r));
        items.push_back(rlp::encodeString(s));
        Bytes out = {0x02};
        Bytes list = rlp::encodeList(items);
        out.insert(out.end(), list.begin(), list.end());
        return out;
    }

    // Legacy EIP-155: keep fields 0..5, replace chainId,"","" with v,r,s.
    std::vector<Bytes> raw;
    if (!splitRaw(rawTx, 0, raw) || raw.size() < 7) return {};
    EvmFields f = decodeFields(rawTx);
    uint64_t v = static_cast<uint64_t>(recid) + 35 + 2 * beToU64(f.chainId);
    std::vector<Bytes> items(raw.begin(), raw.begin() + 6);  // nonce..data, already encoded
    items.push_back(rlp::encodeString(u64ToBE(v)));
    items.push_back(rlp::encodeString(r));
    items.push_back(rlp::encodeString(s));
    return rlp::encodeList(items);
}

SigningItem EvmChain::messageSigningItem(const Bytes& msg, const DerivationPath& path) const {
    // EIP-191 personal_sign: keccak256("\x19Ethereum Signed Message:\n" + len + msg).
    std::string prefix = std::string("\x19") + "Ethereum Signed Message:\n" + std::to_string(msg.size());
    Bytes pre(prefix.begin(), prefix.end());
    pre.insert(pre.end(), msg.begin(), msg.end());
    uint8_t digest[32];
    keccak(pre, digest);
    SigningItem it;
    it.path = path;
    it.curve = Curve::Secp256k1;
    it.payload.assign(digest, digest + 32);
    it.prehashed = true;
    return it;
}

MsgPreview EvmChain::decodeMessage(const Bytes& msg, MsgKind kind) const {
    MsgPreview p;
    if (kind == MsgKind::Personal) {
        // personal_sign: usually UTF-8 text.
        p.text.assign(msg.begin(), msg.end());
    } else {
        p.text = toHex(msg.data(), msg.size() > 32 ? 32 : msg.size());
        p.blindSign = true;
    }
    return p;
}

}  // namespace nema::wallet
