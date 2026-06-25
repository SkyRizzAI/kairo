// Host unit test — EvmChain (Plan 94, Fase 2) against the canonical EIP-155 vector.
//
// EIP-155 example: nonce=9, gasPrice=20 gwei, gas=21000, to=0x3535…35, value=1 ETH,
// data=∅, chainId=1, privkey=0x46×32. The spec publishes both the signing hash and the
// fully signed transaction — so this verifies RLP encoding, the keccak sighash, and
// signature assembly end-to-end against an authoritative reference.

#include "nema/wallet/chains/evm.h"
#include "nema/wallet/chains/rlp.h"
#include "nema/wallet/network_registry.h"
#include "nema/wallet/hd_wallet.h"

#include <cstdio>
#include <string>

extern "C" {
#include "ecdsa.h"
#include "secp256k1.h"

#include <cstdlib>
uint32_t random32(void) { return (uint32_t)arc4random(); }
void random_buffer(uint8_t* b, size_t n) { for (size_t i = 0; i < n; i++) b[i] = (uint8_t)arc4random(); }
}

using namespace nema::wallet;

static int g_fails = 0;
static const char* kHex = "0123456789abcdef";
static std::string hex(const Bytes& b) { std::string s; for (uint8_t x : b) { s += kHex[x >> 4]; s += kHex[x & 0xf]; } return s; }
static void eq(const char* name, const std::string& got, const std::string& exp) {
    bool ok = got == exp; if (!ok) g_fails++;
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) { std::printf("      got %s\n      exp %s\n", got.c_str(), exp.c_str()); }
}
static void ok(const char* name, bool c) { if (!c) g_fails++; std::printf("[%s] %s\n", c ? "PASS" : "FAIL", name); }

int main() {
    EvmChain evm;
    const NetworkParams* eth = NetworkRegistry::find("eth-mainnet");
    ok("network registry: eth-mainnet (chainId 1)", eth && eth->chainId == 1);
    ok("network registry: bnb (chainId 56)", NetworkRegistry::find("bnb") &&
       NetworkRegistry::find("bnb")->chainId == 56);

    // Build the unsigned EIP-155 preimage.
    Bytes to(20, 0x35);
    auto S = [](const Bytes& b) { return rlp::encodeString(b); };
    Bytes rawTx = rlp::encodeList({
        S(Bytes{0x09}),                                            // nonce 9
        S(Bytes{0x04, 0xa8, 0x17, 0xc8, 0x00}),                    // gasPrice 20 gwei
        S(Bytes{0x52, 0x08}),                                      // gas 21000
        S(to),                                                     // to
        S(Bytes{0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64, 0x00, 0x00}),  // value 1e18
        S(Bytes{}),                                                // data
        S(Bytes{0x01}),                                            // chainId 1
        S(Bytes{}), S(Bytes{}),                                    // EIP-155 placeholders
    });

    // Signing hash
    auto items = evm.buildSigningPayload(rawTx, *eth, evm.pathFor(0, *eth));
    ok("one signing item, secp256k1, prehashed",
       items.size() == 1 && items[0].curve == Curve::Secp256k1 && items[0].prehashed);
    // (Signing hash is cross-checked by the signed-tx assembly below: signing THIS hash
    //  with the spec key reproduces the published EIP-155 signature exactly.)
    eq("EIP-155 signing hash", hex(items[0].payload),
       "daf5a779ae972f972197303d7b574746c7ef83eadac0f2791ad23db92e4c8e53");

    // Sign with the spec private key (0x46×32) and assemble.
    uint8_t key[32]; for (int i = 0; i < 32; i++) key[i] = 0x46;
    uint8_t sig64[64], recid = 0;
    ok("ecdsa_sign_digest", ecdsa_sign_digest(&secp256k1, key, items[0].payload.data(), sig64, &recid, nullptr) == 0);
    Signature sig(sig64, sig64 + 64); sig.push_back(recid);

    Bytes signedTx = evm.encodeSigned(rawTx, {sig}, *eth);
    eq("EIP-155 signed transaction", hex(signedTx),
       "f86c098504a817c800825208943535353535353535353535353535353535353535"
       "880de0b6b3a76400008025a028ef61340bd939bc2195fe537567866003e1a15d3c"
       "71ff63e1590620aa636276a067cbe9d8997f761aecb703304b3800ccf555c9f3dc"
       "64214b297fb1966a3b6d83");

    // Address derivation through HdWallet + EvmChain (canonical mnemonic vector).
    {
        HdWallet w;
        w.unlockFromMnemonic(
            "abandon abandon abandon abandon abandon abandon abandon "
            "abandon abandon abandon abandon about", "");
        PubKey pub;
        w.publicKey(evm.pathFor(0, *eth), Curve::Secp256k1, pub);
        eq("EVM address (m/44'/60'/0'/0/0)", evm.addressFromPubkey(pub, *eth),
           "0x9858effd232b4033e47d90003d41ec34ecaeda94");
    }

    // decodeForDisplay sanity
    {
        TxPreview p = evm.decodeForDisplay(rawTx, *eth);
        bool hasTo = false, amt1 = false;
        for (auto& r : p.rows) {
            if (r.label == "To" && r.value == "0x3535353535353535353535353535353535353535") hasTo = true;
            if (r.label == "Amount" && r.value == "1") amt1 = true;
        }
        ok("decode shows To + Amount=1 ETH, not blind", hasTo && amt1 && !p.blindSign);
    }

    std::printf("\n%s (%d failure%s)\n", g_fails ? "EVM CHAIN TESTS FAILED" : "ALL EVM CHAIN TESTS PASS",
                g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
