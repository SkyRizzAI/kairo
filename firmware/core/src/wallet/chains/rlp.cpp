#include "nema/wallet/chains/rlp.h"

namespace nema::wallet::rlp {

namespace {

// Minimal big-endian length, no leading zeros.
Bytes minimalBE(size_t v) {
    Bytes out;
    while (v) { out.insert(out.begin(), static_cast<uint8_t>(v & 0xff)); v >>= 8; }
    return out;
}

void appendHeader(Bytes& out, size_t len, uint8_t shortBase, uint8_t longBase) {
    if (len <= 55) {
        out.push_back(static_cast<uint8_t>(shortBase + len));
    } else {
        Bytes le = minimalBE(len);
        out.push_back(static_cast<uint8_t>(longBase + le.size()));
        out.insert(out.end(), le.begin(), le.end());
    }
}

}  // namespace

Bytes encodeString(const uint8_t* data, size_t n) {
    Bytes out;
    if (n == 1 && data[0] < 0x80) {
        out.push_back(data[0]);  // single low byte encodes as itself
        return out;
    }
    appendHeader(out, n, 0x80, 0xb7);
    out.insert(out.end(), data, data + n);
    return out;
}

Bytes encodeList(const std::vector<Bytes>& encodedItems) {
    Bytes payload;
    for (const auto& it : encodedItems) payload.insert(payload.end(), it.begin(), it.end());
    Bytes out;
    appendHeader(out, payload.size(), 0xc0, 0xf7);
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

bool decodeList(const Bytes& in, std::vector<Bytes>& itemsOut) {
    itemsOut.clear();
    if (in.empty()) return false;

    size_t i = 0;
    uint8_t b = in[i];
    size_t payloadStart, payloadEnd;
    if (b >= 0xf8) {                              // long list
        size_t ll = b - 0xf7;
        if (i + 1 + ll > in.size()) return false;
        size_t len = 0;
        for (size_t k = 0; k < ll; k++) len = (len << 8) | in[i + 1 + k];
        payloadStart = i + 1 + ll;
        payloadEnd = payloadStart + len;
    } else if (b >= 0xc0) {                       // short list
        payloadStart = i + 1;
        payloadEnd = payloadStart + (b - 0xc0);
    } else {
        return false;                             // not a list
    }
    if (payloadEnd > in.size()) return false;

    size_t p = payloadStart;
    while (p < payloadEnd) {
        uint8_t h = in[p];
        size_t contentStart, contentLen;
        if (h <= 0x7f) {                          // single byte
            contentStart = p; contentLen = 1;
        } else if (h <= 0xb7) {                   // short string
            contentStart = p + 1; contentLen = h - 0x80;
        } else if (h <= 0xbf) {                   // long string
            size_t ll = h - 0xb7;
            if (p + 1 + ll > payloadEnd) return false;
            contentLen = 0;
            for (size_t k = 0; k < ll; k++) contentLen = (contentLen << 8) | in[p + 1 + k];
            contentStart = p + 1 + ll;
        } else if (h <= 0xf7) {                   // short list (opaque inner content)
            contentStart = p + 1; contentLen = h - 0xc0;
        } else {                                  // long list
            size_t ll = h - 0xf7;
            if (p + 1 + ll > payloadEnd) return false;
            contentLen = 0;
            for (size_t k = 0; k < ll; k++) contentLen = (contentLen << 8) | in[p + 1 + k];
            contentStart = p + 1 + ll;
        }
        if (contentStart + contentLen > payloadEnd) return false;
        itemsOut.emplace_back(in.begin() + contentStart, in.begin() + contentStart + contentLen);
        p = contentStart + contentLen;
    }
    return p == payloadEnd;
}

}  // namespace nema::wallet::rlp
