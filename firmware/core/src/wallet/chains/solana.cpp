#include "nema/wallet/chains/solana.h"

#include <cstring>

namespace nema::wallet {

namespace {

const char* kB58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// SystemProgram id = 32 zero bytes; Transfer instruction = u32 LE tag 2 + u64 LE lamports.
bool isSystemProgram(const uint8_t* id) {
    for (int i = 0; i < 32; i++) if (id[i] != 0) return false;
    return true;
}

// compact-u16 (shortvec) decode.
bool readCompactU16(const Bytes& b, size_t& off, uint16_t& out) {
    uint32_t val = 0;
    int shift = 0;
    for (int i = 0; i < 3; i++) {
        if (off >= b.size()) return false;
        uint8_t c = b[off++];
        val |= static_cast<uint32_t>(c & 0x7f) << shift;
        if (!(c & 0x80)) { out = static_cast<uint16_t>(val); return true; }
        shift += 7;
    }
    out = static_cast<uint16_t>(val);
    return true;
}

void writeCompactU16(Bytes& b, uint16_t v) {
    for (;;) {
        uint8_t c = v & 0x7f;
        v >>= 7;
        if (v) { b.push_back(c | 0x80); } else { b.push_back(c); break; }
    }
}

std::string lamportsToSol(uint64_t lamports) {
    std::string d = std::to_string(lamports);
    const int dec = 9;
    while (static_cast<int>(d.size()) <= dec) d.insert(d.begin(), '0');
    d.insert(d.end() - dec, '.');
    while (d.back() == '0') d.pop_back();
    if (d.back() == '.') d.pop_back();
    return d;
}

}  // namespace

std::string base58Encode(const uint8_t* data, size_t len) {
    std::vector<uint8_t> digits;  // base58, little-endian
    for (size_t i = 0; i < len; i++) {
        int carry = data[i];
        for (size_t j = 0; j < digits.size(); j++) {
            carry += digits[j] * 256;
            digits[j] = carry % 58;
            carry /= 58;
        }
        while (carry) { digits.push_back(carry % 58); carry /= 58; }
    }
    std::string out;
    for (size_t i = 0; i < len && data[i] == 0; i++) out += '1';  // leading zero bytes → '1'
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) out += kB58[*it];
    return out.empty() ? "1" : out;
}

ChainInfo SolanaChain::info() const { return {"solana", 501, Curve::Ed25519}; }

DerivationPath SolanaChain::pathFor(uint32_t index, const NetworkParams&) const {
    constexpr uint32_t H = DerivationPath::Hardened;
    DerivationPath p;
    p.indices = {44u | H, 501u | H, index | H, 0u | H};  // m/44'/501'/index'/0' (all hardened)
    return p;
}

std::string SolanaChain::addressFromPubkey(const PubKey& pub, const NetworkParams&) const {
    if (pub.size() != 32) return "";
    return base58Encode(pub.data(), 32);
}

TxPreview SolanaChain::decodeForDisplay(const Bytes& m, const NetworkParams& net) const {
    TxPreview p;
    p.rows.push_back({"Network", net.label});

    size_t off = 0;
    if (m.size() < 3) { p.blindSign = true; p.warning = "Unparseable message"; return p; }
    off += 3;  // header: numReqSig, numReadonlySigned, numReadonlyUnsigned

    uint16_t nAccts = 0;
    if (!readCompactU16(m, off, nAccts) || off + static_cast<size_t>(nAccts) * 32 + 32 > m.size()) {
        p.blindSign = true; p.warning = "Bad account table"; return p;
    }
    size_t accountsOff = off;
    off += static_cast<size_t>(nAccts) * 32;
    off += 32;  // recentBlockhash

    auto account = [&](size_t idx) -> const uint8_t* { return m.data() + accountsOff + idx * 32; };

    uint16_t nInstr = 0;
    if (!readCompactU16(m, off, nInstr)) { p.blindSign = true; return p; }

    bool clearSigned = false;
    for (uint16_t ins = 0; ins < nInstr && off < m.size(); ins++) {
        uint8_t progIdx = m[off++];
        uint16_t nAcc = 0;
        if (!readCompactU16(m, off, nAcc) || off + nAcc > m.size()) { p.blindSign = true; break; }
        std::vector<uint8_t> accIdx(m.begin() + off, m.begin() + off + nAcc);
        off += nAcc;
        uint16_t dLen = 0;
        if (!readCompactU16(m, off, dLen) || off + dLen > m.size()) { p.blindSign = true; break; }
        const uint8_t* data = m.data() + off;
        off += dLen;

        // SystemProgram::Transfer — clear-sign.
        if (progIdx < nAccts && isSystemProgram(account(progIdx)) && dLen == 12 &&
            data[0] == 2 && data[1] == 0 && data[2] == 0 && data[3] == 0 && nAcc >= 2 &&
            accIdx[0] < nAccts && accIdx[1] < nAccts) {
            uint64_t lamports = 0;
            for (int i = 7; i >= 0; i--) lamports = (lamports << 8) | data[4 + i];  // u64 LE
            p.rows.push_back({"Type", "Transfer"});
            p.rows.push_back({"To", base58Encode(account(accIdx[1]), 32)});
            p.rows.push_back({"Amount", lamportsToSol(lamports) + " SOL"});
            clearSigned = true;
        }
    }

    if (!clearSigned) {
        p.blindSign = true;
        if (p.warning.empty()) p.warning = "Program call — not decoded";
    }
    return p;
}

std::vector<SigningItem> SolanaChain::buildSigningPayload(const Bytes& rawTx, const NetworkParams&,
                                                          const DerivationPath& account) const {
    SigningItem item;
    item.path = account;
    item.curve = Curve::Ed25519;
    item.payload = rawTx;     // Ed25519 signs the whole message
    item.prehashed = false;
    return {item};
}

Bytes SolanaChain::encodeSigned(const Bytes& rawTx, const std::vector<Signature>& sigs,
                                const NetworkParams&) const {
    if (rawTx.empty() || sigs.empty()) return {};
    uint8_t numReqSig = rawTx[0];
    if (sigs.size() < numReqSig) return {};
    Bytes tx;
    writeCompactU16(tx, numReqSig);
    for (uint8_t i = 0; i < numReqSig; i++) {
        if (sigs[i].size() < 64) return {};
        tx.insert(tx.end(), sigs[i].begin(), sigs[i].begin() + 64);
    }
    tx.insert(tx.end(), rawTx.begin(), rawTx.end());
    return tx;
}

SigningItem SolanaChain::messageSigningItem(const Bytes& msg, const DerivationPath& path) const {
    SigningItem it;
    it.path = path;
    it.curve = Curve::Ed25519;
    it.payload = msg;       // Ed25519 signs the raw message
    it.prehashed = false;
    return it;
}

MsgPreview SolanaChain::decodeMessage(const Bytes& msg, MsgKind) const {
    MsgPreview p;
    p.text.assign(msg.begin(), msg.end());  // Solana off-chain messages are usually UTF-8
    return p;
}

}  // namespace nema::wallet
