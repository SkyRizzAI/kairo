#include "nema/skyrizze32/se050_driver.h"
#include "nema/skyrizze32/board_config.h"
#include "nema/skyrizze32/xl9535.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include <Wire.h>
#include <Arduino.h>
#include <cstring>

// Real NXP SE050 bring-up over T=1'oI2C (NXP AN12413) + the SE05x IoT-applet APDU set
// (public Plug-and-Trust command map). This is FIRST-ITERATION hardware code: every TX
// and RX frame is logged as hex so a single flash reveals the chip's actual behaviour,
// and the seed-sealing feature is gated behind a runtime wrap→unwrap self-test
// (selfTestSeal) — if anything is off, secureStore_ stays false and the wallet falls
// back to software (fail-closed; the seed is never trusted to a half-working SE).

namespace nema::skyrizze32 {

namespace {
constexpr uint8_t CLA        = 0x80;
constexpr uint8_t INS_WRITE  = 0x01, INS_CRYPTO = 0x03, INS_MGMT = 0x04;
constexpr uint8_t P1_DEFAULT = 0x00, P1_AES = 0x03, P1_CIPHER = 0x0E;
constexpr uint8_t P2_DEFAULT = 0x00, P2_RANDOM = 0x49, P2_EXIST = 0x26;
constexpr uint8_t P2_ENC_OS  = 0x37, P2_DEC_OS = 0x38;
constexpr uint8_t TAG_1 = 0x41, TAG_2 = 0x42, TAG_3 = 0x43, TAG_4 = 0x44;
constexpr uint8_t AES_CBC_NOPAD = 0x0D;
// SE05x IoT applet AID.
const uint8_t kAID[] = {0xA0,0x00,0x00,0x03,0x96,0x54,0x53,0x00,0x00,0x00,0x01,0x03,0x00,0x00,0x00,0x00};

std::string hx(const uint8_t* d, size_t n) {
    static const char* k = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n && i < 64; i++) { s += k[d[i] >> 4]; s += k[d[i] & 0xf]; }
    if (n > 64) s += "…";
    return s;
}
void putObjId(std::vector<uint8_t>& v, uint32_t id) {
    v.push_back((id >> 24) & 0xff); v.push_back((id >> 16) & 0xff);
    v.push_back((id >> 8) & 0xff);  v.push_back(id & 0xff);
}
}  // namespace

// CRC-16/X-25 (poly 0x1021 reflected = 0x8408, init 0xFFFF, refin/refout, xorout 0xFFFF).
uint16_t Se050Driver::crc16(const uint8_t* d, size_t n) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        crc ^= d[i];
        for (int b = 0; b < 8; b++) crc = (crc & 1) ? (crc >> 1) ^ 0x8408 : (crc >> 1);
    }
    return crc ^ 0xFFFF;
}

bool Se050Driver::writeBlock(uint8_t pcb, const uint8_t* inf, size_t n) {
    std::vector<uint8_t> f;
    f.reserve(n + 4);
    f.push_back(pcb);
    f.push_back((uint8_t)n);
    if (inf && n) f.insert(f.end(), inf, inf + n);
    uint16_t c = crc16(f.data(), f.size());
    f.push_back(c & 0xff);              // LSB first
    f.push_back((c >> 8) & 0xff);
    rt_->log().debug("SE050", "tx", {{"frame", hx(f.data(), f.size())}});
    Wire.beginTransmission(I2C_ADDR_SE050);
    Wire.write(f.data(), f.size());
    return Wire.endTransmission() == 0;
}

bool Se050Driver::readBlock(uint8_t& pcb, std::vector<uint8_t>& inf) {
    // The chip returns 0xFF while busy; poll until a real block shows up.
    for (int attempt = 0; attempt < 200; attempt++) {
        size_t got = Wire.requestFrom((int)I2C_ADDR_SE050, 2);
        if (got >= 2) {
            uint8_t b0 = Wire.read(), b1 = Wire.read();
            if (b0 != 0xFF) {
                pcb = b0;
                uint8_t len = b1;
                size_t need = (size_t)len + 2;            // INF + CRC
                Wire.requestFrom((int)I2C_ADDR_SE050, (int)need);
                std::vector<uint8_t> rest;
                while (Wire.available()) rest.push_back(Wire.read());
                std::vector<uint8_t> full = {pcb, len};
                full.insert(full.end(), rest.begin(), rest.end());
                rt_->log().debug("SE050", "rx", {{"frame", hx(full.data(), full.size())}});
                if (rest.size() < need) return false;
                inf.assign(rest.begin(), rest.begin() + len);
                uint16_t got_crc = rest[len] | (rest[len + 1] << 8);
                std::vector<uint8_t> chk = {pcb, len};
                chk.insert(chk.end(), inf.begin(), inf.end());
                if (crc16(chk.data(), chk.size()) != got_crc) {
                    rt_->log().warn("SE050", "rx CRC mismatch");
                    return false;
                }
                return true;
            }
        }
        delay(1);
    }
    rt_->log().warn("SE050", "rx timeout (chip stayed busy)");
    return false;
}

bool Se050Driver::softResetAndCIP() {
    if (!writeBlock(0xC0 | 0x0F, nullptr, 0)) return false;   // S-block SOFT_RESET
    uint8_t pcb; std::vector<uint8_t> inf;
    if (!readBlock(pcb, inf)) return false;
    rt_->log().info("SE050", "CIP", {{"pcb", hx(&pcb, 1)}, {"data", hx(inf.data(), inf.size())}});
    seqBit_ = false;
    return true;
}

bool Se050Driver::transceive(const uint8_t* apdu, size_t n, std::vector<uint8_t>& resp, uint16_t& sw) {
    uint8_t pcb = seqBit_ ? 0x40 : 0x00;   // I-block, toggle N(S)
    seqBit_ = !seqBit_;
    if (!writeBlock(pcb, apdu, n)) return false;
    uint8_t rpcb; std::vector<uint8_t> inf;
    for (int guard = 0; guard < 4; guard++) {
        if (!readBlock(rpcb, inf)) return false;
        if ((rpcb & 0xC0) == 0xC0) {       // S-block (e.g. WTX) — ack and keep waiting
            writeBlock(0xE0 | (rpcb & 0x3F), inf.data(), inf.size());
            continue;
        }
        break;                             // I-block response
    }
    if (inf.size() < 2) return false;
    sw = (inf[inf.size() - 2] << 8) | inf[inf.size() - 1];
    resp.assign(inf.begin(), inf.end() - 2);
    return true;
}

bool Se050Driver::selectApplet() {
    std::vector<uint8_t> a = {0x00, 0xA4, 0x04, 0x00, (uint8_t)sizeof(kAID)};
    a.insert(a.end(), kAID, kAID + sizeof(kAID));
    a.push_back(0x00);
    std::vector<uint8_t> r; uint16_t sw = 0;
    bool ok = transceive(a.data(), a.size(), r, sw) && sw == 0x9000;
    rt_->log().info("SE050", "selectApplet", {{"sw", hx((uint8_t*)&sw, 2)}, {"ok", ok ? "1" : "0"}});
    return ok;
}

bool Se050Driver::getRandom(uint8_t* out, size_t n) {
    std::vector<uint8_t> data = {TAG_1, 0x02, (uint8_t)(n >> 8), (uint8_t)(n & 0xff)};
    std::vector<uint8_t> a = {CLA, INS_MGMT, P1_DEFAULT, P2_RANDOM, (uint8_t)data.size()};
    a.insert(a.end(), data.begin(), data.end());
    a.push_back(0x00);
    std::vector<uint8_t> r; uint16_t sw = 0;
    if (!transceive(a.data(), a.size(), r, sw) || sw != 0x9000) return false;
    if (r.size() < 2 || r[0] != TAG_1) return false;
    uint8_t len = r[1];
    if (r.size() < 2u + len || len < n) return false;
    std::memcpy(out, r.data() + 2, n);
    return true;
}

bool Se050Driver::ensureAesKey(uint32_t objId) {
    {   // exists?
        std::vector<uint8_t> data = {TAG_1, 0x04}; putObjId(data, objId);
        std::vector<uint8_t> a = {CLA, INS_MGMT, P1_DEFAULT, P2_EXIST, (uint8_t)data.size()};
        a.insert(a.end(), data.begin(), data.end()); a.push_back(0x00);
        std::vector<uint8_t> r; uint16_t sw = 0;
        if (transceive(a.data(), a.size(), r, sw) && sw == 0x9000 && r.size() >= 3 && r[2] == 0x01) {
            rt_->log().info("SE050", "seal key exists");
            return true;
        }
    }
    // create: 32 chip-random bytes → persistent AES-256 object
    uint8_t key[32];
    if (!getRandom(key, sizeof(key))) return false;
    std::vector<uint8_t> data = {TAG_1, 0x04}; putObjId(data, objId);
    data.push_back(TAG_3); data.push_back(sizeof(key));
    data.insert(data.end(), key, key + sizeof(key));
    std::memset(key, 0, sizeof(key));
    std::vector<uint8_t> a = {CLA, INS_WRITE, P1_AES, P2_DEFAULT, (uint8_t)data.size()};
    a.insert(a.end(), data.begin(), data.end());
    std::vector<uint8_t> r; uint16_t sw = 0;
    bool ok = transceive(a.data(), a.size(), r, sw) && sw == 0x9000;
    rt_->log().info("SE050", "create seal key", {{"sw", hx((uint8_t*)&sw, 2)}, {"ok", ok ? "1" : "0"}});
    return ok;
}

bool Se050Driver::cipher(uint32_t objId, bool enc, const uint8_t* iv,
                         const uint8_t* in, size_t n, std::vector<uint8_t>& out) {
    std::vector<uint8_t> data;
    data.push_back(TAG_1); data.push_back(0x04); putObjId(data, objId);
    data.push_back(TAG_2); data.push_back(0x01); data.push_back(AES_CBC_NOPAD);
    data.push_back(TAG_3); data.push_back((uint8_t)n); data.insert(data.end(), in, in + n);
    data.push_back(TAG_4); data.push_back(0x10); data.insert(data.end(), iv, iv + 16);
    std::vector<uint8_t> a = {CLA, INS_CRYPTO, P1_CIPHER, (uint8_t)(enc ? P2_ENC_OS : P2_DEC_OS),
                              (uint8_t)data.size()};
    a.insert(a.end(), data.begin(), data.end()); a.push_back(0x00);
    std::vector<uint8_t> r; uint16_t sw = 0;
    if (!transceive(a.data(), a.size(), r, sw) || sw != 0x9000) return false;
    if (r.size() < 2 || r[0] != TAG_1) return false;
    uint8_t len = r[1];
    if (r.size() < 2u + len) return false;
    out.assign(r.begin() + 2, r.begin() + 2 + len);
    return true;
}

// ── ISecureElement ──────────────────────────────────────────────────────────

bool Se050Driver::randomBytes(uint8_t* out, size_t n) {
    return present_ && getRandom(out, n);
}

bool Se050Driver::uniqueId(std::string& /*out*/) const {
    return false;   // TODO: read the factory unique-ID object
}

// wrap: out = iv(16) || AES-256-CBC(sealKey, iv, in). `n` must be a multiple of 16.
bool Se050Driver::wrap(uint8_t, const uint8_t* in, size_t n, std::vector<uint8_t>& out) {
    if (!secureStore_ || n == 0 || n % 16 != 0) return false;
    uint8_t iv[16];
    if (!getRandom(iv, sizeof(iv))) return false;
    std::vector<uint8_t> ct;
    if (!cipher(aesObjId_, true, iv, in, n, ct)) return false;
    out.assign(iv, iv + 16);
    out.insert(out.end(), ct.begin(), ct.end());
    return true;
}

bool Se050Driver::unwrap(uint8_t, const uint8_t* in, size_t n, std::vector<uint8_t>& out) {
    if (!secureStore_ || n < 32 || (n - 16) % 16 != 0) return false;
    return cipher(aesObjId_, false, in, in + 16, n - 16, out);
}

bool Se050Driver::selfTestSeal() {
    uint8_t plain[32];
    for (int i = 0; i < 32; i++) plain[i] = (uint8_t)(i * 7 + 1);
    secureStore_ = true;                       // enable wrap/unwrap for the test only
    std::vector<uint8_t> sealed, back;
    bool ok = wrap(0, plain, sizeof(plain), sealed) &&
              unwrap(0, sealed.data(), sealed.size(), back) &&
              back.size() == sizeof(plain) && std::memcmp(back.data(), plain, sizeof(plain)) == 0;
    secureStore_ = ok;                         // keep it on ONLY if the round-trip matched
    return ok;
}

void Se050Driver::init(Runtime& rt, Xl9535& expander) {
    rt_       = &rt;
    expander_ = &expander;

    // Reset pulse via XL9535 P03, then let the chip boot.
    expander.setSeReset(true);  delay(2);
    expander.setSeReset(false); delay(10);

    Wire.beginTransmission(I2C_ADDR_SE050);
    present_ = (Wire.endTransmission() == 0);
    if (!present_) {
        rt.log().warn("SE050", "no ACK on I2C — absent or held in reset", {{"addr", "0x48"}});
        return;
    }
    // Gate the T=1'oI2C bring-up while the protocol is still being validated against the
    // real chip. Set to true to attempt sealing; false = presence-only (old scaffold,
    // known-safe boot). The first-iteration framing isn't correct yet (two-requestFrom
    // reads → CRC mismatch), so keep it off until the transport is fixed + heap-clean.
    constexpr bool kEnableBringup = false;
    if (!kEnableBringup) {
        rt.log().info("SE050", "present (bring-up gated off — wallet stays software)",
                      {{"addr", "0x48"}});
        secureStore_ = false;
        return;
    }
    rt.log().info("SE050", "present — starting T=1'oI2C bring-up", {{"addr", "0x48"}});

    // Bring-up; each step logs. Any failure → secureStore_ stays false → software (safe).
    bool reset  = softResetAndCIP();
    bool select = reset && selectApplet();
    uint8_t rnd[8];
    bool rng    = select && getRandom(rnd, sizeof(rnd));
    if (rng) rt.log().info("SE050", "GetRandom OK (real hardware TRNG)", {{"sample", hx(rnd, 8)}});
    bool keyOk  = rng && ensureAesKey(aesObjId_);
    bool seal   = keyOk && selfTestSeal();

    secureStore_ = seal;
    rt.log().info("SE050", "bring-up result",
                  {{"reset", reset ? "1" : "0"}, {"select", select ? "1" : "0"},
                   {"rng", rng ? "1" : "0"}, {"sealKey", keyOk ? "1" : "0"},
                   {"selfTest", seal ? "1" : "0"},
                   {"secureStore", secureStore_ ? "ENABLED (wallet -> mode B)"
                                                : "disabled (wallet -> software)"}});
}

} // namespace nema::skyrizze32
