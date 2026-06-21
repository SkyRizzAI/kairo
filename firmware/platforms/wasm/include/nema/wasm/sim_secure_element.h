#pragma once
#include "nema/hal/secure_element.h"
#include <cstdint>

namespace nema {

// SimSecureElement — software-emulated secure element for the WASM simulator.
//
// There is no SE050 in the browser, so this stands in for one. It is modelled on
// the most capable part (SE050E-class) so wallet code can be developed against
// the full curve set — secp256k1 (BTC/EVM) and Ed25519 (Solana) — in the sim.
//
// SCAFFOLD (ADR 0005): randomBytes/uniqueId are functional (deterministic, sim
// only — NOT cryptographically secure). The key operations (genKey/sign/verify)
// are TODO until the proven software crypto core (trezor-crypto: secp256k1 /
// ed25519 / nist256p1) is vendored in the wallet phase; they return false so
// callers don't trust a fake signature.
class SimSecureElement : public ISecureElement {
public:
    const char* name() const override { return "SimSE"; }

    uint8_t slotCount() const override { return 16; }

    // Emulate SE050E: every wallet curve is "supported" in the sim.
    bool supportsKeyType(SeKeyType) const override { return true; }

    // Deterministic LCG fill — reproducible across sim runs, NOT secure.
    bool randomBytes(uint8_t* out, size_t n) override {
        if (!out) return false;
        for (size_t i = 0; i < n; ++i) {
            seed_ = seed_ * 6364136223846793005ull + 1442695040888963407ull;
            out[i] = static_cast<uint8_t>(seed_ >> 56);
        }
        return true;
    }

    bool uniqueId(std::string& out) const override {
        out = "53494d2d53450001";   // "SIM-SE\0\1" in hex — fixed sim serial
        return true;
    }

    // ── Operations — TODO pending trezor-crypto (ADR 0005) ──
    bool genKey(uint8_t, SeKeyType) override { return false; }
    bool publicKey(uint8_t, std::vector<uint8_t>&) const override { return false; }
    bool sign(uint8_t, const uint8_t*, size_t, std::vector<uint8_t>&) override { return false; }
    bool verify(uint8_t, const uint8_t*, size_t, const uint8_t*, size_t) const override { return false; }

    // Sim emulates the SE050's secure object store.
    bool hasFeature(SeFeature f) const override { return f == SeFeature::SecureStore; }

private:
    uint64_t seed_ = 0x9e3779b97f4a7c15ull;   // fixed → deterministic sim
};

} // namespace nema
