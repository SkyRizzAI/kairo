#pragma once
#include "nema/hal/driver.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace nema {

// Key types a secure element can hold.
//
// Only EccP256 (NIST secp256r1) is the GUARANTEED baseline — it is the common
// denominator across every chip Palanu ships (SE050 and ATECC608B both do P-256).
//
// The wallet curves are CHIP-SPECIFIC and must be probed with supportsKeyType()
// before genKey() — no crypto-wallet chain uses P-256:
//   Secp256k1 → Bitcoin / EVM   — SE050 yes, ATECC608B NO.
//   Ed25519   → Solana          — SE050E variant only, ATECC608B NO.
// A board whose chip lacks the curve returns false; the wallet must then keep
// that key in its software keystore instead.
enum class SeKeyType { EccP256, Secp256k1, Ed25519 };

// Optional, chip-specific feature flags. The genKey/sign/verify P-256 baseline
// is guaranteed on every implementation; anything here is queried via
// hasFeature() before the corresponding (not-yet-declared) calls are used.
enum class SeFeature { SecureStore, Rsa, Aes, Attestation };

// ISecureElement — hardware root-of-trust abstraction.
//
// One interface, many chips: a board with an SE050 and a board with an
// ATECC608B both expose this. Apps NEVER branch on the chip — they check
// caps::Secure, resolve<ISecureElement>() from the container, and call. Private
// keys are generated and used INSIDE the chip and never leave it; that is the
// whole point of the part, and the reason this is hardware-backed rather than
// the software crypto in nema/crypto/sha256.h.
//
// All operations return false on failure (no exceptions). Slots are small
// integer handles into the chip's key store; the valid range is chip-specific —
// query slotCount(). Apps that need a secure element but run on a board without
// one should gate on caps::Secure and fall back to software crypto.
struct ISecureElement : IDriver {
    DriverKind kind() const override { return DriverKind::Other; }

    // ── Baseline: every secure element implements these ──

    // Hardware TRNG. Fills out[0..n). false if the chip refused / errored.
    virtual bool randomBytes(uint8_t* out, size_t n) = 0;

    // Immutable factory-provisioned unique id (serial), as a hex string.
    virtual bool uniqueId(std::string& out) const = 0;

    // Number of usable key slots on this chip.
    virtual uint8_t slotCount() const = 0;

    // Probe curve support before genKey(). EccP256 is always true; the wallet
    // curves (Secp256k1 / Ed25519) are chip-specific.
    virtual bool supportsKeyType(SeKeyType type) const {
        return type == SeKeyType::EccP256;
    }

    // Generate a fresh keypair in `slot`. Overwrites any existing key there.
    virtual bool genKey(uint8_t slot, SeKeyType type) = 0;

    // Export the PUBLIC key for `slot` (the private half never leaves the chip).
    // Format: uncompressed SEC1 (0x04 || X || Y), 65 bytes for P-256.
    virtual bool publicKey(uint8_t slot, std::vector<uint8_t>& out) const = 0;

    // ECDSA-sign `digest` (already hashed — 32 bytes for P-256) with the slot's
    // private key. sig is filled with raw r||s (64 bytes for P-256).
    virtual bool sign(uint8_t slot, const uint8_t* digest, size_t digestLen,
                      std::vector<uint8_t>& sig) = 0;

    // Verify `sig` over `digest` against the slot's public key.
    virtual bool verify(uint8_t slot, const uint8_t* digest, size_t digestLen,
                        const uint8_t* sig, size_t sigLen) const = 0;

    // ── Optional, chip-specific ──

    // Query before relying on any non-baseline capability. Default: none.
    virtual bool hasFeature(SeFeature) const { return false; }
};

} // namespace nema
