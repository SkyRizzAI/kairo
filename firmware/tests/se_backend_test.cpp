// Host unit test — SeBackend mode B (Plan 94, Fase 4).
//
// Proves the secure-element custody path against SoftSecureElement: create → PIN-encrypt
// → device-wrap → store; unlock needs the right PIN AND the right device; the indicator
// reads SecureElement; and the seed survives the round-trip (canonical ETH address).

#include "nema/wallet/backends/se_backend.h"
#include "nema/wallet/soft_secure_element.h"

#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
#include "ecdsa.h"
#include "secp256k1.h"
#include "sha3.h"

#include <cstdlib>
uint32_t random32(void) { return (uint32_t)arc4random(); }
void random_buffer(uint8_t* b, size_t n) { for (size_t i = 0; i < n; i++) b[i] = (uint8_t)arc4random(); }
}

using namespace nema::wallet;

static int g_fails = 0;
static void ok(const char* name, bool c) { if (!c) g_fails++; std::printf("[%s] %s\n", c ? "PASS" : "FAIL", name); }

namespace {
struct MemStore : ISeedStore {
    std::vector<uint8_t> blob; bool has = false;
    bool save(const uint8_t* b, size_t n) override { blob.assign(b, b + n); has = true; return true; }
    bool load(std::vector<uint8_t>& o) override { if (!has) return false; o = blob; return true; }
    bool exists() const override { return has; }
    void erase() override { has = false; }
};

std::string ethAddr(IWalletBackend& be) {
    DerivationPath p; constexpr uint32_t H = DerivationPath::Hardened;
    p.indices = {44u | H, 60u | H, 0u | H, 0u, 0u};
    PubKey pub; if (!be.publicKey(p, Curve::Secp256k1, pub)) return "";
    uint8_t pub65[65]; ecdsa_uncompress_pubkey(&secp256k1, pub.data(), pub65);
    uint8_t kh[32]; keccak_256(pub65 + 1, 64, kh);
    char a[43]; a[0] = '0'; a[1] = 'x';
    for (int i = 0; i < 20; i++) std::sprintf(a + 2 + 2 * i, "%02x", kh[12 + i]);
    return std::string(a);
}
const char* kMnemonic =
    "abandon abandon abandon abandon abandon abandon abandon "
    "abandon abandon abandon abandon about";
const char* kExpEth = "0x9858effd232b4033e47d90003d41ec34ecaeda94";
}  // namespace

int main() {
    nema::SoftSecureElement se;  // device A
    MemStore store;
    SeBackend be(se, store);

    ok("SE supports mode B (SecureStore)", SeBackend::supportedBy(se));
    ok("create", be.create(kMnemonic, "", "1234"));
    ok("indicator = SecureElement", be.kind() == BackendKind::SecureElement);
    ok("ready + hasWallet", be.ready() && be.hasWallet());
    ok("ETH addr after create", ethAddr(be) == kExpEth);

    be.lock();
    ok("wrong PIN rejected", !be.unlock("9999"));
    ok("correct PIN unlocks", be.unlock("1234"));
    ok("ETH addr after unlock", ethAddr(be) == kExpEth);

    // Device-bound: the SAME stored blob must NOT unlock on a DIFFERENT device key.
    {
        uint8_t otherKey[32];
        std::memset(otherKey, 0x5C, 32);
        nema::SoftSecureElement se2(otherKey);  // device B
        SeBackend be2(se2, store);              // same store/blob, different chip
        ok("different device cannot unlock (device-bound)", !be2.unlock("1234"));
    }

    be.wipe();
    ok("wipe clears wallet", !be.hasWallet());

    std::printf("\n%s (%d failure%s)\n", g_fails ? "SE BACKEND TESTS FAILED" : "ALL SE BACKEND TESTS PASS",
                g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
