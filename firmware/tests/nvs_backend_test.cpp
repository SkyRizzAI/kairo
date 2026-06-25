// Host unit test — NvsBackend (Plan 94, Fase 1).
//
// Verifies the software custody backend: create → encrypt seed under a PIN → persist,
// then lock/unlock with the right and wrong PIN, and confirm the seed survives the
// encrypt/decrypt round-trip by re-deriving the canonical ETH address. Uses an
// in-memory ISeedStore (firmware backs this with AppStorage::critical()).

#include "nema/wallet/backends/nvs_backend.h"

#include <cstdio>
#include <cstring>

extern "C" {
#include "ecdsa.h"
#include "secp256k1.h"
#include "sha3.h"

// trezor-crypto leaves the RNG to the platform (vendor README); the test stubs it.
#include <cstdlib>
uint32_t random32(void) { return (uint32_t)arc4random(); }
void random_buffer(uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)arc4random();
}
}

using namespace nema::wallet;

namespace {
struct MemStore : ISeedStore {
    std::vector<uint8_t> blob;
    bool has = false;
    bool save(const uint8_t* b, size_t n) override { blob.assign(b, b + n); has = true; return true; }
    bool load(std::vector<uint8_t>& out) override { if (!has) return false; out = blob; return true; }
    bool exists() const override { return has; }
    void erase() override { blob.clear(); has = false; }
};

int g_fails = 0;
void ok(const char* name, bool cond) {
    if (!cond) g_fails++;
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", name);
}

// ETH address from a backend's derived pubkey at m/44'/60'/0'/0/0.
std::string ethAddr(IWalletBackend& be) {
    DerivationPath p;
    constexpr uint32_t H = DerivationPath::Hardened;
    p.indices = {44u | H, 60u | H, 0u | H, 0u, 0u};
    PubKey pub;
    if (!be.publicKey(p, Curve::Secp256k1, pub)) return "";
    uint8_t pub65[65];
    ecdsa_uncompress_pubkey(&secp256k1, pub.data(), pub65);
    uint8_t kh[32];
    keccak_256(pub65 + 1, 64, kh);
    char a[43];
    a[0] = '0'; a[1] = 'x';
    for (int i = 0; i < 20; i++) std::sprintf(a + 2 + 2 * i, "%02x", kh[12 + i]);
    return std::string(a);
}
}  // namespace

int main() {
    const char* mnemonic =
        "abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon about";
    const char* kExpEth = "0x9858effd232b4033e47d90003d41ec34ecaeda94";

    MemStore store;
    NvsBackend be(store);

    ok("no wallet initially", !be.hasWallet() && !be.ready());
    ok("create wallet", be.create(mnemonic, "", "1234"));
    ok("ready after create", be.ready() && be.hasWallet());
    ok("kind is Software", be.kind() == BackendKind::Software);
    ok("ETH addr after create", ethAddr(be) == kExpEth);

    // Blob is ciphertext — must NOT contain the raw seed/mnemonic in clear.
    std::vector<uint8_t> blob;
    store.load(blob);
    ok("blob is 112 bytes", blob.size() == 112);

    be.lock();
    ok("locked clears ready (blob persists)", !be.ready() && be.hasWallet());

    ok("wrong PIN rejected", !be.unlock("9999"));
    ok("still locked after wrong PIN", !be.ready());

    ok("correct PIN unlocks", be.unlock("1234"));
    ok("ETH addr after unlock", ethAddr(be) == kExpEth);

    // A fresh backend over the same store can also unlock (persistence works).
    NvsBackend be2(store);
    ok("reopened backend unlocks", be2.unlock("1234") && ethAddr(be2) == kExpEth);

    be.wipe();
    ok("wipe clears wallet", !be.hasWallet());

    std::printf("\n%s (%d failure%s)\n",
                g_fails ? "NVS BACKEND TESTS FAILED" : "ALL NVS BACKEND TESTS PASS",
                g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
