#pragma once
#include "nema/hal/secure_element.h"
#include <cstdint>

namespace nema {
class Runtime;
}

namespace nema::skyrizzsolana {

// Se050Driver — NXP SE050C2 secure element on SkyRizz Solana.
//
// Shared I²C bus (GPIO9/10 @ 0x48), enabled via direct GPIO8 (SE_EN, HIGH=on) —
// no reset line on this board. Talks to the chip through the vendored NXP Plug &
// Trust nano-package (component `se05x`): T=1'oI2C + APDU, plain session (no
// SCP03). Used for mode-B seed sealing — a persistent in-chip AES-256 key
// wrap/unwraps the wallet's seed blob, so recovery needs BOTH the PIN and this
// physical chip. Signing stays in software (this variant + nano-package don't
// expose secp256k1 in-chip). Mirrors the E32 driver (Plan 96).
//
// Fail-closed: hasFeature(SecureStore) is true ONLY after a boot-time wrap→unwrap
// self-test round-trips on the real chip — a non-working SE never makes the
// wallet trust it (it falls back to software).
class Se050Driver : public ISecureElement {
public:
    void init(Runtime& rt);     // enable + open session + self-test
    void deinit();               // close the SE session

    const char* name() const override { return "SE050"; }
    bool present() const { return present_; }

    uint8_t slotCount() const override { return 16; }
    bool supportsKeyType(SeKeyType type) const override {
        // SE050C2 (Weierstrass): NIST P-256 + secp256k1. Ed25519 = SE050E only.
        return type == SeKeyType::EccP256 || type == SeKeyType::Secp256k1;
    }

    bool hasFeature(SeFeature f) const override {
        return f == SeFeature::SecureStore && secureStore_;
    }

    // ── ISecureElement: operations (via nano-package) ──
    bool randomBytes(uint8_t* out, size_t n) override;
    bool uniqueId(std::string& out) const override;
    bool wrap(uint8_t slot, const uint8_t* in, size_t n, std::vector<uint8_t>& out) override;
    bool unwrap(uint8_t slot, const uint8_t* in, size_t n, std::vector<uint8_t>& out) override;

    // In-chip key-gen/signing not used (signing stays software; SE only seals the seed).
    bool genKey(uint8_t, SeKeyType) override { return false; }
    bool publicKey(uint8_t, std::vector<uint8_t>&) const override { return false; }
    bool sign(uint8_t, const uint8_t*, size_t, std::vector<uint8_t>&) override { return false; }
    bool verify(uint8_t, const uint8_t*, size_t, const uint8_t*, size_t) const override { return false; }

private:
    bool openSession();
    bool ensureAesKey();
    bool cipher(bool encrypt, const uint8_t* iv,
                const uint8_t* in, size_t n, std::vector<uint8_t>& out);
    bool selfTestSeal();

    Runtime* rt_          = nullptr;
    bool     present_     = false;
    bool     sessionOpen_ = false;
    bool     secureStore_ = false;
    uint32_t aesObjId_    = 0xA5A50001;   // fixed SE object id for the wallet seal key
};

} // namespace nema::skyrizzsolana
