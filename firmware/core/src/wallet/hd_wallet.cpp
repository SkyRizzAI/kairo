#include "nema/wallet/hd_wallet.h"

#include <cstring>

extern "C" {
#include "bip39.h"
#include "bip32.h"
#include "ecdsa.h"
#include "hasher.h"
#include "memzero.h"
#include "rand.h"
}

namespace nema::wallet {

namespace {

const char* curveName(Curve c) {
    return c == Curve::Ed25519 ? "ed25519" : "secp256k1";
}

// Derive an HDNode at `path` for `curve` from `seed`. Indices already carry the
// hardened bit (DerivationPath convention). ed25519 only supports hardened children
// — a non-hardened index there makes hdnode_private_ckd fail, which we propagate.
bool deriveNode(const uint8_t seed[64], const DerivationPath& path, Curve curve, HDNode& node) {
    if (hdnode_from_seed(seed, 64, curveName(curve), &node) != 1) return false;
    for (uint32_t idx : path.indices) {
        if (hdnode_private_ckd(&node, idx) != 1) {
            memzero(&node, sizeof(node));
            return false;
        }
    }
    hdnode_fill_public_key(&node);
    return true;
}

}  // namespace

HdWallet::~HdWallet() { lock(); }

void HdWallet::lock() {
    memzero(seed_, sizeof(seed_));
    ready_ = false;
}

bool HdWallet::generateMnemonic(int strengthBits, std::string& out) {
    // Valid BIP39 entropy sizes: 128/160/192/224/256 bits (multiples of 32).
    if (strengthBits < 128 || strengthBits > 256 || strengthBits % 32 != 0) return false;
    const int bytes = strengthBits / 8;
    uint8_t entropy[32];
    random_buffer(entropy, bytes);  // platform RNG (see vendor README)
    const char* m = mnemonic_from_data(entropy, bytes);
    memzero(entropy, sizeof(entropy));
    if (!m) return false;
    out = m;
    mnemonic_clear();  // wipe trezor's internal static buffer
    return !out.empty();
}

bool HdWallet::validateMnemonic(const std::string& mnemonic) {
    return mnemonic_check(mnemonic.c_str()) != 0;
}

bool HdWallet::unlockFromMnemonic(const std::string& mnemonic, const std::string& passphrase) {
    if (!validateMnemonic(mnemonic)) return false;
    mnemonic_to_seed(mnemonic.c_str(), passphrase.c_str(), seed_, nullptr);
    ready_ = true;
    return true;
}

bool HdWallet::unlockFromSeed(const uint8_t* seed, size_t n) {
    if (n != sizeof(seed_) || seed == nullptr) return false;
    std::memcpy(seed_, seed, sizeof(seed_));
    ready_ = true;
    return true;
}

bool HdWallet::publicKey(const DerivationPath& path, Curve curve, PubKey& out) const {
    if (!ready_) return false;
    HDNode node;
    if (!deriveNode(seed_, path, curve, node)) return false;
    if (curve == Curve::Ed25519)
        out.assign(node.public_key + 1, node.public_key + 33);  // strip the 0x00 prefix → 32 bytes
    else
        out.assign(node.public_key, node.public_key + 33);      // compressed SEC1
    memzero(&node, sizeof(node));
    return true;
}

bool HdWallet::sign(const DerivationPath& path, Curve curve,
                    const uint8_t* payload, size_t n, bool prehashed, Signature& out) const {
    if (!ready_ || payload == nullptr) return false;
    HDNode node;
    if (!deriveNode(seed_, path, curve, node)) return false;

    bool ok = false;
    if (curve == Curve::Secp256k1) {
        // ECDSA over a pre-hashed 32-byte digest; RFC6979 + low-S inside trezor.
        if (prehashed && n == 32) {
            uint8_t sig[64];
            uint8_t recid = 0;
            if (ecdsa_sign_digest(node.curve->params, node.private_key, payload, sig, &recid,
                                  nullptr) == 0) {
                out.assign(sig, sig + 64);
                out.push_back(recid);  // r||s||recid — EVM derives v, BTC ignores it
                memzero(sig, sizeof(sig));
                ok = true;
            }
        }
    } else {  // Ed25519 — pure EdDSA over the whole message (must NOT be pre-hashed).
        if (!prehashed) {
            uint8_t sig[64];
            // hasher_sign is ignored for ed25519 nodes (EdDSA hashes internally);
            // pass a valid HasherType to satisfy the signature.
            if (hdnode_sign(&node, payload, static_cast<uint32_t>(n), HASHER_SHA2, sig, nullptr,
                            nullptr) == 0) {
                out.assign(sig, sig + 64);
                memzero(sig, sizeof(sig));
                ok = true;
            }
        }
    }

    memzero(&node, sizeof(node));
    return ok;
}

bool HdWallet::privateKey(const DerivationPath& path, Curve curve, Bytes& out) const {
    if (!ready_) return false;
    HDNode node;
    if (!deriveNode(seed_, path, curve, node)) return false;
    out.assign(node.private_key, node.private_key + 32);  // raw 32-byte private key
    memzero(&node, sizeof(node));
    return true;
}

}  // namespace nema::wallet
