// Host unit test — BitcoinChain (Plan 94, Fase 2): address derivation (BIP84 P2WPKH).
// Tx signing (BIP143) is a verified follow-up; here we lock down the app-critical
// receive path against the official BIP84 vector + testnet HRP.

#include "nema/wallet/chains/bitcoin.h"
#include "nema/wallet/network_registry.h"
#include "nema/wallet/hd_wallet.h"

#include <cstdio>
#include <string>

extern "C" {
#include <cstdlib>
uint32_t random32(void) { return (uint32_t)arc4random(); }
void random_buffer(uint8_t* b, size_t n) { for (size_t i = 0; i < n; i++) b[i] = (uint8_t)arc4random(); }
}

using namespace nema::wallet;

static int g_fails = 0;
static void ok(const char* name, bool c) { if (!c) g_fails++; std::printf("[%s] %s\n", c ? "PASS" : "FAIL", name); }

int main() {
    BitcoinChain btc;
    const NetworkParams* main = NetworkRegistry::find("btc-mainnet");
    const NetworkParams* test = NetworkRegistry::find("btc-testnet");
    ok("registry btc-mainnet/testnet", main && test && test->testnet);
    ok("info: bitcoin/coin0/secp256k1",
       std::string(btc.info().family) == "bitcoin" && btc.info().bip44CoinType == 0 &&
       btc.info().curve == Curve::Secp256k1);

    HdWallet w;
    w.unlockFromMnemonic(
        "abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon about", "");

    // BIP84 official first receive address: m/84'/0'/0'/0/0
    PubKey pub;
    bool got = w.publicKey(btc.pathFor(0, *main), Curve::Secp256k1, pub);
    ok("compressed pubkey (33)", got && pub.size() == 33);
    std::string addr = btc.addressFromPubkey(pub, *main);
    ok("BIP84 mainnet address", addr == "bc1qcr8te4kr609gcawutmrza0j4xv80jy8z306fyu");
    std::printf("      btc address = %s\n", addr.c_str());

    // Testnet uses the 'tb' HRP.
    std::string taddr = btc.addressFromPubkey(pub, *test);
    ok("testnet HRP 'tb1'", taddr.rfind("tb1", 0) == 0);

    std::printf("\n%s (%d failure%s)\n", g_fails ? "BITCOIN CHAIN TESTS FAILED" : "ALL BITCOIN CHAIN TESTS PASS",
                g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
