#pragma once
#include "nema/wallet/wallet_types.h"

// IWalletBackend — CUSTODY axis (Plan 94, ADR 0015). Curve-level, chain-blind.

namespace nema::wallet {

// Holds/derives keys from a seed and signs. NEVER exposes the private key or seed
// (ADR 0014): BIP32/SLIP-0010 derivation happens INSIDE the backend, so the seed
// never crosses this boundary. A chain driver supplies the path + payload; the
// backend just signs — it knows curves, not chains.
//
// Implementations:
//   NvsBackend — seed encrypted in software (PIN-derived key), no SE. kind()=Software.
//   SeBackend  — seed wrapped by the SE050 (mode B).               kind()=SecureElement.
struct IWalletBackend {
    virtual ~IWalletBackend() = default;

    // Custody of the keys this backend holds — drives the UI trust indicator.
    virtual BackendKind kind() const = 0;

    // Whether a usable seed exists right now (wallet created/restored AND unlocked).
    virtual bool ready() const = 0;

    // Derive the PUBLIC key at `path` for `curve`. The private half never leaves.
    virtual bool publicKey(const DerivationPath& path, Curve curve, PubKey& out) = 0;

    // Sign ONE payload. See SigningItem for the prehashed convention:
    //   secp256k1 (ECDSA): payload = 32-byte digest, prehashed=true, RFC6979 + low-S.
    //   ed25519   (EdDSA): payload = whole message,  prehashed=false.
    // Returns false on failure (locked / no key / refused). Never returns key material.
    virtual bool sign(const DerivationPath& path, Curve curve,
                      const uint8_t* payload, size_t n, bool prehashed,
                      Signature& out) = 0;

    // ── Custody lifecycle (uniform across NvsBackend/SeBackend so WalletController is
    //    backend-agnostic). create/unlock take the PIN; the passphrase is baked into the
    //    stored seed at create time, so unlock needs only the PIN. ──
    virtual bool hasWallet() const = 0;
    virtual bool create(const std::string& mnemonic, const std::string& passphrase,
                        const std::string& pin) = 0;
    virtual bool unlock(const std::string& pin) = 0;
    virtual void lock() = 0;
    virtual void wipe() = 0;
};

} // namespace nema::wallet
