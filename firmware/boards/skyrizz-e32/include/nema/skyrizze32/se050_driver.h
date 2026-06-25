#pragma once
#include "nema/hal/secure_element.h"
#include <cstdint>

namespace nema {
class Runtime;
}

namespace nema::skyrizze32 {

class Xl9535;

// Se050Driver — NXP SE050 secure element on the SkyRizz E32 (U18).
//
// Shared I²C bus (GPIO47/48 @ 0x48), reset via XL9535 P03 (P0_SE_RST). This is
// the hardware root-of-trust backend for the generic ISecureElement HAL — apps
// gate on caps::Secure, never on the board.
//
// SCAFFOLD (ADR 0005): init() does the real bring-up — reset pulse + I²C
// presence probe — and the capability/curve probes below report the SE050
// silicon honestly (so wallet code can be developed against the real target).
// The crypto OPERATIONS (genKey/sign/verify/...) are TODO: they require the NXP
// Plug-and-Trust middleware (APDU session over I²C) and physical hardware to
// validate against, so they return false for now and log a warning.
class Se050Driver : public ISecureElement {
public:
    // Reset pulse via the expander + I²C presence probe. Call from
    // describeHardware() before registering the capability.
    void init(Runtime& rt, Xl9535& expander);

    const char* name() const override { return "SE050"; }

    // True once the chip ACKed on the I²C bus during init().
    bool present() const { return present_; }

    // ── ISecureElement: curve / feature probes (reflect the silicon) ──
    uint8_t slotCount() const override { return 16; }   // SE050 has many key objects
    bool supportsKeyType(SeKeyType type) const override {
        // SE050C2 (Weierstrass curves): NIST P-256 + secp256k1. Ed25519 is SE050E-only.
        return type == SeKeyType::EccP256 || type == SeKeyType::Secp256k1;
    }

    // Device-bound sealing (mode B). Reported true ONLY after init()'s wrap→unwrap
    // self-test actually round-tripped on the real chip — so a half-working bring-up
    // can never make the wallet trust the SE (fail-closed; it falls back to software).
    bool hasFeature(SeFeature f) const override {
        return f == SeFeature::SecureStore && secureStore_;
    }

    // ── ISecureElement: operations (real T=1'oI2C + SE05x APDU) ──
    bool randomBytes(uint8_t* out, size_t n) override;
    bool uniqueId(std::string& out) const override;
    bool wrap(uint8_t slot, const uint8_t* in, size_t n, std::vector<uint8_t>& out) override;
    bool unwrap(uint8_t slot, const uint8_t* in, size_t n, std::vector<uint8_t>& out) override;

    // Still hardware-TODO (need in-chip key-gen/signing; the wallet signs in software
    // and only seals the seed via wrap/unwrap above).
    bool genKey(uint8_t, SeKeyType) override { return false; }
    bool publicKey(uint8_t, std::vector<uint8_t>&) const override { return false; }
    bool sign(uint8_t, const uint8_t*, size_t, std::vector<uint8_t>&) override { return false; }
    bool verify(uint8_t, const uint8_t*, size_t, const uint8_t*, size_t) const override { return false; }

private:
    // ── T=1'oI2C transport (NXP AN12413) ──
    static uint16_t crc16(const uint8_t* d, size_t n);
    bool writeBlock(uint8_t pcb, const uint8_t* inf, size_t n);
    bool readBlock(uint8_t& pcb, std::vector<uint8_t>& inf);   // polls until ready
    bool softResetAndCIP();                                    // power-on handshake
    // Send one ISO7816 APDU inside an I-block; return response data + SW. ok = SW==0x9000.
    bool transceive(const uint8_t* apdu, size_t n, std::vector<uint8_t>& resp, uint16_t& sw);

    // ── SE05x applet ──
    bool selectApplet();
    bool getRandom(uint8_t* out, size_t n);
    bool ensureAesKey(uint32_t objId);                          // create persistent AES-256 obj if absent
    bool cipher(uint32_t objId, bool encrypt, const uint8_t* iv,
                const uint8_t* in, size_t n, std::vector<uint8_t>& out);
    bool selfTestSeal();                                        // wrap→unwrap a test vector

    Runtime* rt_          = nullptr;
    Xl9535*  expander_    = nullptr;
    bool     present_     = false;   // chip ACKed on I²C
    bool     secureStore_ = false;   // self-test passed → wallet may use mode B
    bool     seqBit_      = false;   // T=1 I-block N(S) toggle
    uint32_t aesObjId_    = 0xA5A50001;  // fixed SE object id for the wallet seal key
};

} // namespace nema::skyrizze32
