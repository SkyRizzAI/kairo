#include "nema/wallet/chains/bitcoin.h"

#include <cstring>
#include <cstdint>
#include <cstdio>
#include <array>
#include <vector>
#include <string>

extern "C" {
#include "hasher.h"
#include "segwit_addr.h"
#include "ecdsa.h"
#include "secp256k1.h"
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

// ── Transaction signing: BIP143 (native SegWit / P2WPKH). ───────────────────────
//
// The wallet is offline, so it can't fetch prevout amounts/scripts — BIP143 needs
// them in the sighash. Since there is no PSBT parser, the app passes a compact,
// explicit "sign request" (Palanu BTC v1) that already carries everything required
// to (a) show the user what they sign and (b) compute every input's sighash:
//
//   u8   0x01                    format version
//   u32  nVersion                (LE)
//   var  nIn
//     per input:
//       [32] prev txid           (tx byte order, as serialized)
//       u32  prev vout           (LE)
//       u64  amount (sats)       (LE)        — required by BIP143
//       var  scriptPubKey len    (P2WPKH = 22: 0x0014<h160>)
//       [..] scriptPubKey
//       u32  nSequence           (LE)
//   var  nOut
//     per output:
//       u64  amount (sats)       (LE)
//       var  scriptPubKey len
//       [..] scriptPubKey
//   u32  nLockTime               (LE)
//
// All inputs are signed with the account key (single-key P2WPKH wallet). The
// algorithm is verified against the official BIP143 P2WPKH vector in
// firmware/tests/bitcoin_chain_test.cpp. We FAIL CLOSED on anything we can't fully
// build (non-P2WPKH input, short blob) — never sign what we can't reproduce.

namespace {

// ── little-endian + varint writers ──
void putU32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((v >> (8 * i)) & 0xff); }
void putU64(Bytes& b, uint64_t v) { for (int i = 0; i < 8; i++) b.push_back((v >> (8 * i)) & 0xff); }
void putVarint(Bytes& b, uint64_t n) {
    if (n < 0xfd) b.push_back((uint8_t)n);
    else if (n <= 0xffff) { b.push_back(0xfd); b.push_back(n & 0xff); b.push_back((n >> 8) & 0xff); }
    else if (n <= 0xffffffffULL) { b.push_back(0xfe); for (int i = 0; i < 4; i++) b.push_back((n >> (8 * i)) & 0xff); }
    else { b.push_back(0xff); for (int i = 0; i < 8; i++) b.push_back((n >> (8 * i)) & 0xff); }
}
void dsha256(const uint8_t* d, size_t n, uint8_t out[32]) { hasher_Raw(HASHER_SHA2D, d, n, out); }

// ── cursor reader over the sign-request blob ──
struct Reader {
    const uint8_t* p; size_t n; size_t i = 0; bool err = false;
    explicit Reader(const Bytes& b) : p(b.data()), n(b.size()) {}
    uint8_t  u8()  { if (i >= n) { err = true; return 0; } return p[i++]; }
    uint32_t u32() { uint32_t v = 0; for (int k = 0; k < 4; k++) v |= (uint32_t)u8() << (8 * k); return v; }
    uint64_t u64() { uint64_t v = 0; for (int k = 0; k < 8; k++) v |= (uint64_t)u8() << (8 * k); return v; }
    uint64_t varint() {
        uint8_t f = u8();
        if (f < 0xfd) return f;
        if (f == 0xfd) { uint64_t v = u8(); v |= (uint64_t)u8() << 8; return v; }
        if (f == 0xfe) { uint64_t v = 0; for (int k = 0; k < 4; k++) v |= (uint64_t)u8() << (8 * k); return v; }
        uint64_t v = 0; for (int k = 0; k < 8; k++) v |= (uint64_t)u8() << (8 * k); return v;
    }
    void read(uint8_t* dst, size_t len) { for (size_t k = 0; k < len; k++) { uint8_t c = u8(); if (dst) dst[k] = c; } }
    Bytes take(size_t len) { Bytes b; b.reserve(len); for (size_t k = 0; k < len && !err; k++) b.push_back(u8()); return b; }
};

struct BtcIn  { uint8_t txid[32]; uint32_t vout; uint64_t amount; Bytes spk; uint32_t sequence; };
struct BtcOut { uint64_t amount; Bytes spk; };
struct BtcTx  { uint32_t version = 0; std::vector<BtcIn> vin; std::vector<BtcOut> vout; uint32_t locktime = 0; bool ok = false; };

BtcTx parseRequest(const Bytes& raw) {
    BtcTx t;
    Reader r(raw);
    if (r.u8() != 0x01) return t;               // format version
    t.version = r.u32();
    uint64_t nIn = r.varint();
    if (nIn == 0 || nIn > 256) return t;
    for (uint64_t k = 0; k < nIn; k++) {
        BtcIn in;
        r.read(in.txid, 32);
        in.vout = r.u32();
        in.amount = r.u64();
        uint64_t sl = r.varint(); if (sl > 1024) return t;
        in.spk = r.take(sl);
        in.sequence = r.u32();
        t.vin.push_back(std::move(in));
    }
    uint64_t nOut = r.varint();
    if (nOut == 0 || nOut > 256) return t;
    for (uint64_t k = 0; k < nOut; k++) {
        BtcOut o;
        o.amount = r.u64();
        uint64_t sl = r.varint(); if (sl > 1024) return t;
        o.spk = r.take(sl);
        t.vout.push_back(std::move(o));
    }
    t.locktime = r.u32();
    t.ok = !r.err;
    return t;
}

// P2WPKH scriptCode for the BIP143 preimage: 0x19 76a914 <20-byte h160> 88ac
// (length-prefixed). The keyhash comes from the input's witness program (0014<h160>).
bool scriptCodeForInput(const BtcIn& in, Bytes& out) {
    if (in.spk.size() == 22 && in.spk[0] == 0x00 && in.spk[1] == 0x14) {
        out = {0x19, 0x76, 0xa9, 0x14};
        out.insert(out.end(), in.spk.begin() + 2, in.spk.begin() + 22);
        out.push_back(0x88);
        out.push_back(0xac);
        return true;
    }
    return false;  // only P2WPKH inputs are supported
}

// BIP143 sighash (SIGHASH_ALL) for every input. false if any input is not P2WPKH.
bool computeSighashes(const BtcTx& t, std::vector<std::array<uint8_t, 32>>& out) {
    Bytes pre; for (auto& in : t.vin) { pre.insert(pre.end(), in.txid, in.txid + 32); putU32(pre, in.vout); }
    uint8_t hashPrevouts[32]; dsha256(pre.data(), pre.size(), hashPrevouts);

    Bytes seq; for (auto& in : t.vin) putU32(seq, in.sequence);
    uint8_t hashSequence[32]; dsha256(seq.data(), seq.size(), hashSequence);

    Bytes outs;
    for (auto& o : t.vout) { putU64(outs, o.amount); putVarint(outs, o.spk.size()); outs.insert(outs.end(), o.spk.begin(), o.spk.end()); }
    uint8_t hashOutputs[32]; dsha256(outs.data(), outs.size(), hashOutputs);

    out.clear();
    for (auto& in : t.vin) {
        Bytes sc; if (!scriptCodeForInput(in, sc)) return false;
        Bytes m;
        putU32(m, t.version);
        m.insert(m.end(), hashPrevouts, hashPrevouts + 32);
        m.insert(m.end(), hashSequence, hashSequence + 32);
        m.insert(m.end(), in.txid, in.txid + 32); putU32(m, in.vout);  // outpoint
        m.insert(m.end(), sc.begin(), sc.end());                       // scriptCode (len-prefixed)
        putU64(m, in.amount);
        putU32(m, in.sequence);
        m.insert(m.end(), hashOutputs, hashOutputs + 32);
        putU32(m, t.locktime);
        putU32(m, 0x00000001);                                         // SIGHASH_ALL
        std::array<uint8_t, 32> h; dsha256(m.data(), m.size(), h.data());
        out.push_back(h);
    }
    return true;
}

// sats → "X.YYYYYYYY" (trailing zeros trimmed).
std::string satsToBtc(uint64_t sats) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%llu.%08llu",
                  (unsigned long long)(sats / 100000000ULL),
                  (unsigned long long)(sats % 100000000ULL));
    std::string s(buf);
    size_t dot = s.find('.');
    if (dot != std::string::npos) {
        size_t last = s.find_last_not_of('0');
        if (last > dot) s.erase(last + 1); else s.erase(dot);
    }
    return s;
}

// Output scriptPubKey → bech32 address when it's a witness-v0 program, else hex.
std::string outAddr(const Bytes& spk, const NetworkParams& net) {
    if (spk.size() >= 4 && spk[0] == 0x00 && spk[1] == spk.size() - 2) {
        const char* hrp = net.testnet ? "tb" : "bc";
        char a[100];
        if (segwit_addr_encode(a, hrp, 0, spk.data() + 2, spk.size() - 2) == 1) return std::string(a);
    }
    static const char* hx = "0123456789abcdef";
    std::string h; for (uint8_t b : spk) { h.push_back(hx[b >> 4]); h.push_back(hx[b & 15]); }
    return h;
}

}  // namespace

TxPreview BitcoinChain::decodeForDisplay(const Bytes& raw, const NetworkParams& net) const {
    TxPreview p;
    p.rows.push_back({"Network", net.label});
    BtcTx t = parseRequest(raw);
    if (!t.ok || t.vin.empty() || t.vout.empty()) {
        p.blindSign = true; p.warning = "Unparseable Bitcoin tx"; return p;
    }
    std::vector<std::array<uint8_t, 32>> hs;
    if (!computeSighashes(t, hs)) { p.blindSign = true; p.warning = "Only P2WPKH inputs supported"; return p; }

    uint64_t totIn = 0;  for (auto& in : t.vin)  totIn  += in.amount;
    uint64_t totOut = 0; for (auto& o : t.vout)  totOut += o.amount;
    for (auto& o : t.vout) {
        p.rows.push_back({"To", outAddr(o.spk, net)});
        p.rows.push_back({"Amount", satsToBtc(o.amount) + " BTC"});
    }
    if (totIn >= totOut) p.rows.push_back({"Fee", satsToBtc(totIn - totOut) + " BTC"});
    return p;
}

std::vector<SigningItem> BitcoinChain::buildSigningPayload(const Bytes& raw, const NetworkParams&,
                                                           const DerivationPath& account) const {
    BtcTx t = parseRequest(raw);
    std::vector<std::array<uint8_t, 32>> hs;
    if (!t.ok || !computeSighashes(t, hs)) return {};  // fail closed
    std::vector<SigningItem> items;
    for (auto& h : hs) {
        SigningItem it;
        it.path = account;
        it.curve = Curve::Secp256k1;
        it.prehashed = true;
        it.payload.assign(h.begin(), h.end());
        items.push_back(std::move(it));
    }
    return items;
}

Bytes BitcoinChain::encodeSigned(const Bytes& raw, const std::vector<Signature>& sigs,
                                 const NetworkParams&) const {
    BtcTx t = parseRequest(raw);
    std::vector<std::array<uint8_t, 32>> hs;
    if (!t.ok || !computeSighashes(t, hs)) return {};
    if (sigs.size() < t.vin.size()) return {};

    Bytes tx;
    putU32(tx, t.version);
    tx.push_back(0x00); tx.push_back(0x01);          // SegWit marker + flag
    putVarint(tx, t.vin.size());
    for (auto& in : t.vin) {
        tx.insert(tx.end(), in.txid, in.txid + 32);
        putU32(tx, in.vout);
        tx.push_back(0x00);                          // empty scriptSig (native segwit)
        putU32(tx, in.sequence);
    }
    putVarint(tx, t.vout.size());
    for (auto& o : t.vout) {
        putU64(tx, o.amount);
        putVarint(tx, o.spk.size());
        tx.insert(tx.end(), o.spk.begin(), o.spk.end());
    }
    for (size_t k = 0; k < t.vin.size(); k++) {
        const Signature& sig = sigs[k];
        if (sig.size() < 65) return {};              // need r||s||recid
        uint8_t pub65[65];
        if (ecdsa_recover_pub_from_sig(&secp256k1, pub65, sig.data(), hs[k].data(), sig[64]) != 0) return {};
        uint8_t pub33[33]; pub33[0] = 0x02 | (pub65[64] & 1); std::memcpy(pub33 + 1, pub65 + 1, 32);
        uint8_t der[72]; int dl = ecdsa_sig_to_der(sig.data(), der);
        if (dl <= 0) return {};
        tx.push_back(0x02);                          // 2 witness stack items
        putVarint(tx, (uint64_t)dl + 1);
        tx.insert(tx.end(), der, der + dl);
        tx.push_back(0x01);                          // SIGHASH_ALL byte
        putVarint(tx, 33);
        tx.insert(tx.end(), pub33, pub33 + 33);
    }
    putU32(tx, t.locktime);
    return tx;
}

MsgPreview BitcoinChain::decodeMessage(const Bytes& msg, MsgKind) const {
    MsgPreview p;
    p.text.assign(msg.begin(), msg.end());  // legacy signmessage text
    return p;
}

SigningItem BitcoinChain::messageSigningItem(const Bytes& msg, const DerivationPath& path) const {
    // Legacy "Bitcoin Signed Message" — the format `bitcoin-cli signmessage` and most
    // wallets use. Far simpler and more widely verifiable than full BIP-322:
    //   preimage = varint(24) ++ "Bitcoin Signed Message:\n" ++ varint(len(msg)) ++ msg
    //   digest   = SHA256d(preimage)
    // We sign that 32-byte digest with secp256k1 (RFC6979 + low-S, like every other
    // ECDSA path here).
    static const char kMagic[] = "Bitcoin Signed Message:\n";  // exactly 24 bytes
    Bytes pre;
    auto pushVarint = [&](uint64_t n) {
        if (n < 0xfd) { pre.push_back((uint8_t)n); }
        else if (n <= 0xffff) { pre.push_back(0xfd); pre.push_back(n & 0xff); pre.push_back((n >> 8) & 0xff); }
        else { pre.push_back(0xfe); for (int i = 0; i < 4; i++) pre.push_back((n >> (8 * i)) & 0xff); }
    };
    pushVarint(24);
    pre.insert(pre.end(), kMagic, kMagic + 24);
    pushVarint(msg.size());
    pre.insert(pre.end(), msg.begin(), msg.end());

    uint8_t digest[32];
    hasher_Raw(HASHER_SHA2D, pre.data(), pre.size(), digest);  // double SHA-256

    SigningItem it;
    it.path = path;
    it.curve = Curve::Secp256k1;
    it.payload.assign(digest, digest + 32);
    it.prehashed = true;
    return it;
}

}  // namespace nema::wallet
