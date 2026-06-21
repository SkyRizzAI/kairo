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
        // SE050 (Weierstrass curves): NIST P-256 + secp256k1. Ed25519 is only on
        // the SE050E variant — gated off until the board's exact part is confirmed.
        return type == SeKeyType::EccP256 || type == SeKeyType::Secp256k1;
    }

    // ── ISecureElement: operations — TODO pending Plug-and-Trust middleware ──
    bool randomBytes(uint8_t* out, size_t n) override;
    bool uniqueId(std::string& out) const override;
    bool genKey(uint8_t slot, SeKeyType type) override;
    bool publicKey(uint8_t slot, std::vector<uint8_t>& out) const override;
    bool sign(uint8_t slot, const uint8_t* digest, size_t digestLen,
              std::vector<uint8_t>& sig) override;
    bool verify(uint8_t slot, const uint8_t* digest, size_t digestLen,
                const uint8_t* sig, size_t sigLen) const override;

private:
    Runtime* rt_       = nullptr;
    Xl9535*  expander_ = nullptr;
    bool     present_  = false;
};

} // namespace nema::skyrizze32
