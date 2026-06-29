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

static nema::wallet::Bytes hex2bytes(const std::string& h) {
    nema::wallet::Bytes b; b.reserve(h.size() / 2);
    auto nib = [](char c) -> int { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; };
    for (size_t i = 0; i + 1 < h.size(); i += 2) b.push_back((uint8_t)((nib(h[i]) << 4) | nib(h[i + 1])));
    return b;
}
static std::string bytes2hex(const nema::wallet::Bytes& b) {
    static const char* hx = "0123456789abcdef"; std::string s;
    for (uint8_t x : b) { s.push_back(hx[x >> 4]); s.push_back(hx[x & 15]); }
    return s;
}

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

    // ── BIP143 sighash — the official P2WPKH test vector ────────────────────────
    // From BIP143's worked example. We encode it as a Palanu BTC v1 sign-request
    // (input 0's spk/amount don't enter input 1's sighash, so a dummy P2WPKH spk
    // for input 0 is fine). buildSigningPayload()[1] must equal the known digest —
    // a wrong sighash here = lost funds, so this guards the whole signing path.
    std::string req =
        "01"                                                                 // fmt version
        "01000000"                                                           // nVersion
        "02"                                                                 // nIn
        "fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f"   // in0 txid
        "00000000" "00e1f50500000000" "16" "00140000000000000000000000000000000000000000" "eeffffff"
        "ef51e1b804cc89d182d279655c3aa89e815b1b309fe287d9b2b55d57b90ec68a"   // in1 txid
        "01000000" "0046c32300000000" "16" "00141d0f172a0ecb48aee1be1f2687d2963ae33f71a1" "ffffffff"
        "02"                                                                 // nOut
        "202cb20600000000" "19" "76a9148280b37df378db99f66f85c95a783a76ac7a6d5988ac"
        "9093510d00000000" "19" "76a9143bde42dbee7e4dbe6a21b2d50ce2f0167faa815988ac"
        "11000000";                                                          // nLockTime
    auto items = btc.buildSigningPayload(hex2bytes(req), *main, btc.pathFor(0, *main));
    ok("BIP143: 2 signing items", items.size() == 2);
    std::string got1 = items.size() == 2 ? bytes2hex(items[1].payload) : "";
    ok("BIP143 P2WPKH sighash (input 1)",
       got1 == "c37af31116d1b27caf68aae9e3ac82f1477929014d5b917657d0eb49478cb670");
    std::printf("      sighash[1] = %s\n", got1.c_str());

    // decodeForDisplay must be WYSIWYS (not blind) and show both outputs + fee.
    TxPreview pv = btc.decodeForDisplay(hex2bytes(req), *main);
    ok("decode: not blind-sign", !pv.blindSign);
    bool hasFee = false; for (auto& r : pv.rows) if (r.label == "Fee") hasFee = true;
    ok("decode: shows fee row", hasFee);

    std::printf("\n%s (%d failure%s)\n", g_fails ? "BITCOIN CHAIN TESTS FAILED" : "ALL BITCOIN CHAIN TESTS PASS",
                g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
