#pragma once
#include "nema/hal/secure_element.h"

// SoftSecureElement — software ISecureElement for host tests + WASM sim (Plan 94, Fase 4).
//
// Models the SE050's device-bound sealing: wrap()/unwrap() use an in-memory key that
// never appears in the output, so ciphertext produced by one instance cannot be
// unwrapped by another instance with a different key (a different "device"). This lets
// the mode-B SeBackend be developed and tested without the real chip. In-chip key
// generation/signing is NOT modelled — mode B signs in software (HdWallet); the SE only
// guards the seed at rest.

namespace nema {

class SoftSecureElement : public ISecureElement {
public:
    SoftSecureElement();                                  // fixed dev key (deterministic sim)
    explicit SoftSecureElement(const uint8_t deviceKey[32]);

    const char* name() const override { return "SoftSecureElement"; }

    bool randomBytes(uint8_t* out, size_t n) override;
    bool uniqueId(std::string& out) const override;
    uint8_t slotCount() const override { return 1; }
    bool supportsKeyType(SeKeyType) const override { return true; }

    bool genKey(uint8_t, SeKeyType) override { return false; }
    bool publicKey(uint8_t, std::vector<uint8_t>&) const override { return false; }
    bool sign(uint8_t, const uint8_t*, size_t, std::vector<uint8_t>&) override { return false; }
    bool verify(uint8_t, const uint8_t*, size_t, const uint8_t*, size_t) const override { return false; }

    bool hasFeature(SeFeature f) const override { return f == SeFeature::SecureStore; }

    bool wrap(uint8_t slot, const uint8_t* in, size_t n, std::vector<uint8_t>& out) override;
    bool unwrap(uint8_t slot, const uint8_t* in, size_t n, std::vector<uint8_t>& out) override;

private:
    uint8_t key_[32];
};

}  // namespace nema
