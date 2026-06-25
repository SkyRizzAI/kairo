// Host unit test — wallet crypto core (Plan 94, Fase 1).
//
// Proves the vendored trezor-crypto derives correct addresses across all three
// chain families, against AUTHORITATIVE test vectors:
//   ETH — BIP44 secp256k1 + keccak256       (m/44'/60'/0'/0/0)
//   BTC — BIP84 P2WPKH bech32                (m/84'/0'/0'/0/0, official BIP84 vector)
//   SOL — Ed25519 SLIP-0010 spec vector 1    (seed 000102..0f, m/0')
//
// Getting any curve/derivation wrong yields a wrong address (= lost funds), so this
// is the load-bearing correctness gate for the software wallet backend.

#include <cstdio>
#include <cstdint>
#include <cstring>

extern "C" {
#include "bip39.h"
#include "bip32.h"
#include "sha3.h"
#include "ecdsa.h"
#include "hasher.h"
#include "segwit_addr.h"

// trezor-crypto leaves the RNG to the platform (see vendor README). Tests stub it.
#include <cstdlib>
uint32_t random32(void) { return (uint32_t)arc4random(); }
void random_buffer(uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)arc4random();
}
}

static int g_fails = 0;

static void toHex(char* out, const uint8_t* b, int n) {
    for (int i = 0; i < n; i++) std::sprintf(out + 2 * i, "%02x", b[i]);
}

static void check(const char* name, const char* got, const char* exp) {
    bool ok = std::strcmp(got, exp) == 0;
    if (!ok) g_fails++;
    std::printf("[%s] %-4s %s\n", ok ? "PASS" : "FAIL", name, got);
    if (!ok) std::printf("          expected %s\n", exp);
}

int main() {
    const char* mnemonic =
        "abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon about";
    uint8_t seed[64];
    mnemonic_to_seed(mnemonic, "", seed, nullptr);
    char buf[200];

    // A) ETH — BIP44 secp256k1 + keccak256
    {
        HDNode n;
        hdnode_from_seed(seed, 64, "secp256k1", &n);
        uint32_t path[] = {44u | 0x80000000u, 60u | 0x80000000u, 0x80000000u, 0u, 0u};
        for (uint32_t p : path) hdnode_private_ckd(&n, p);
        hdnode_fill_public_key(&n);
        uint8_t pub65[65];
        ecdsa_get_public_key65(n.curve->params, n.private_key, pub65);
        uint8_t kh[32];
        keccak_256(pub65 + 1, 64, kh);
        buf[0] = '0'; buf[1] = 'x';
        toHex(buf + 2, kh + 12, 20);
        check("ETH", buf, "0x9858effd232b4033e47d90003d41ec34ecaeda94");
    }

    // B) BTC — BIP84 P2WPKH bech32 (official BIP84 first receive address)
    {
        HDNode n;
        hdnode_from_seed(seed, 64, "secp256k1", &n);
        uint32_t path[] = {84u | 0x80000000u, 0x80000000u, 0x80000000u, 0u, 0u};
        for (uint32_t p : path) hdnode_private_ckd(&n, p);
        hdnode_fill_public_key(&n);
        uint8_t h160[32];
        hasher_Raw(HASHER_SHA2_RIPEMD, n.public_key, 33, h160);  // hash160
        segwit_addr_encode(buf, "bc", 0, h160, 20);
        check("BTC", buf, "bc1qcr8te4kr609gcawutmrza0j4xv80jy8z306fyu");
    }

    // C) Ed25519 — SLIP-0010 spec vector 1, seed 000102..0f, m/0'
    {
        uint8_t s[16];
        for (int i = 0; i < 16; i++) s[i] = (uint8_t)i;
        HDNode n;
        hdnode_from_seed(s, 16, "ed25519", &n);
        hdnode_private_ckd_prime(&n, 0);
        hdnode_fill_public_key(&n);
        toHex(buf, n.public_key, 33);  // 0x00 || 32-byte pubkey
        check("SOL", buf,
              "008c8a13df77a28f3445213a0f432fde644acaa215fc72dcdf300d5efaa85d350c");
    }

    std::printf("\n%s (%d failure%s)\n",
                g_fails ? "WALLET CRYPTO TESTS FAILED" : "ALL WALLET CRYPTO VECTORS PASS",
                g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
