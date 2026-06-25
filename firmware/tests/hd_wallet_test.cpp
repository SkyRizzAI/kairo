// Host unit test — HdWallet wrapper (Plan 94, Fase 1).
//
// Exercises the C++ engine that both custody backends sit on: mnemonic gen/validate,
// seed unlock, public-key derivation, and signing — verifying the signatures actually
// validate (secp256k1 via ecdsa_verify_digest, ed25519 via ed25519_sign_open) and that
// derivation still matches the canonical ETH vector THROUGH the wrapper.

#include "nema/wallet/hd_wallet.h"

#include <cstdio>
#include <cstring>

extern "C" {
#include "ecdsa.h"
#include "secp256k1.h"
#include "sha3.h"
#include "ed25519-donna/ed25519.h"

// trezor-crypto leaves the RNG to the platform (vendor README); the test stubs it.
#include <cstdlib>
uint32_t random32(void) { return (uint32_t)arc4random(); }
void random_buffer(uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)arc4random();
}
}

using namespace nema::wallet;

static int g_fails = 0;
static void ok(const char* name, bool cond) {
    if (!cond) g_fails++;
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", name);
}

static DerivationPath path(std::initializer_list<uint32_t> v) {
    DerivationPath p;
    p.indices.assign(v.begin(), v.end());
    return p;
}
static constexpr uint32_t H = DerivationPath::Hardened;

int main() {
    // 1) generate + validate a 12-word mnemonic
    std::string gen;
    ok("generate 12-word", HdWallet::generateMnemonic(128, gen) && HdWallet::validateMnemonic(gen));
    ok("reject garbage mnemonic", !HdWallet::validateMnemonic("not a real mnemonic phrase here"));

    const char* mnemonic =
        "abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon about";

    HdWallet w;
    ok("unlock from mnemonic", w.unlockFromMnemonic(mnemonic, ""));

    // 2) ETH address through the wrapper (m/44'/60'/0'/0/0) == canonical vector
    {
        PubKey pub;
        bool got = w.publicKey(path({44u | H, 60u | H, 0u | H, 0u, 0u}), Curve::Secp256k1, pub);
        uint8_t pub65[65];
        ecdsa_uncompress_pubkey(&secp256k1, pub.data(), pub65);
        uint8_t kh[32];
        keccak_256(pub65 + 1, 64, kh);
        char addr[43];
        addr[0] = '0'; addr[1] = 'x';
        for (int i = 0; i < 20; i++) std::sprintf(addr + 2 + 2 * i, "%02x", kh[12 + i]);
        ok("ETH address via wrapper",
           got && std::strcmp(addr, "0x9858effd232b4033e47d90003d41ec34ecaeda94") == 0);
    }

    // 3) secp256k1 sign → 65 bytes, and the r||s half verifies
    {
        DerivationPath p = path({44u | H, 60u | H, 0u | H, 0u, 0u});
        PubKey pub; w.publicKey(p, Curve::Secp256k1, pub);
        uint8_t digest[32];
        keccak_256(reinterpret_cast<const uint8_t*>("hello"), 5, digest);
        Signature sig;
        bool signed_ok = w.sign(p, Curve::Secp256k1, digest, 32, /*prehashed*/ true, sig);
        bool verifies = signed_ok && sig.size() == 65 &&
                        ecdsa_verify_digest(&secp256k1, pub.data(), sig.data(), digest) == 0;
        ok("secp256k1 sign+verify", verifies);
    }

    // 4) ed25519 sign whole message → 64 bytes, verifies against the derived pubkey
    {
        DerivationPath p = path({44u | H, 501u | H, 0u | H, 0u | H});  // Solana
        PubKey pub; w.publicKey(p, Curve::Ed25519, pub);
        const uint8_t msg[] = "solana transaction bytes";
        Signature sig;
        bool signed_ok = w.sign(p, Curve::Ed25519, msg, sizeof(msg), /*prehashed*/ false, sig);
        bool verifies = signed_ok && sig.size() == 64 && pub.size() == 32 &&
                        ed25519_sign_open(msg, sizeof(msg), pub.data(), sig.data()) == 0;
        ok("ed25519 sign+verify", verifies);
    }

    // 5) prehashed-flag misuse is rejected (ed25519 must not be pre-hashed)
    {
        DerivationPath p = path({44u | H, 501u | H, 0u | H, 0u | H});
        uint8_t d[32] = {0};
        Signature sig;
        ok("ed25519 rejects prehashed", !w.sign(p, Curve::Ed25519, d, 32, true, sig));
    }

    // 6) lock wipes the seed
    w.lock();
    ok("lock clears ready", !w.ready());
    {
        PubKey pub;
        ok("derivation fails when locked",
           !w.publicKey(path({44u | H, 60u | H, 0u | H, 0u, 0u}), Curve::Secp256k1, pub));
    }

    std::printf("\n%s (%d failure%s)\n",
                g_fails ? "HD WALLET TESTS FAILED" : "ALL HD WALLET TESTS PASS",
                g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
