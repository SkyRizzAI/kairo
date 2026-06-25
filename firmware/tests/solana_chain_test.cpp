// Host unit test — SolanaChain (Plan 94, Fase 2).
//
// base58 (authoritative zeros→ones vector), SystemProgram::Transfer message decode,
// and a sign→verify→encode round-trip (Ed25519 over the whole message).

#include "nema/wallet/chains/solana.h"
#include "nema/wallet/network_registry.h"
#include "nema/wallet/hd_wallet.h"

#include <cstdio>
#include <string>

extern "C" {
#include "ed25519-donna/ed25519.h"

#include <cstdlib>
uint32_t random32(void) { return (uint32_t)arc4random(); }
void random_buffer(uint8_t* b, size_t n) { for (size_t i = 0; i < n; i++) b[i] = (uint8_t)arc4random(); }
}

using namespace nema::wallet;

static int g_fails = 0;
static void ok(const char* name, bool c) { if (!c) g_fails++; std::printf("[%s] %s\n", c ? "PASS" : "FAIL", name); }

int main() {
    SolanaChain sol;
    const NetworkParams* net = NetworkRegistry::find("sol-mainnet");
    ok("registry sol-mainnet (solana family)", net && std::string(net->family) == "solana");

    // base58: 32 zero bytes → the System program id "111…1" (32 ones).
    {
        uint8_t z[32] = {0};
        ok("base58(32 zeros) = 32×'1'", base58Encode(z, 32) == std::string(32, '1'));
    }

    // Derive our Solana account.
    HdWallet w;
    w.unlockFromMnemonic(
        "abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon about", "");
    DerivationPath path = sol.pathFor(0, *net);
    PubKey from;
    w.publicKey(path, Curve::Ed25519, from);
    ok("ed25519 pubkey is 32 bytes", from.size() == 32);
    std::string addr = sol.addressFromPubkey(from, *net);
    ok("address is non-empty base58 (32–44 chars)", addr.size() >= 32 && addr.size() <= 44);
    std::printf("      sol address = %s\n", addr.c_str());

    // Build a SystemProgram::Transfer message: from → to (0x02×32), 1 SOL.
    Bytes msg;
    msg.push_back(1); msg.push_back(0); msg.push_back(1);   // header
    msg.push_back(3);                                       // 3 accounts
    msg.insert(msg.end(), from.begin(), from.end());        // [0] from
    Bytes to(32, 0x02);
    msg.insert(msg.end(), to.begin(), to.end());            // [1] to
    Bytes sys(32, 0x00);
    msg.insert(msg.end(), sys.begin(), sys.end());          // [2] SystemProgram
    Bytes blockhash(32, 0x01);
    msg.insert(msg.end(), blockhash.begin(), blockhash.end());
    msg.push_back(1);                                       // 1 instruction
    msg.push_back(2);                                       // progIdx = SystemProgram
    msg.push_back(2);                                       // 2 account indices
    msg.push_back(0); msg.push_back(1);                     // from, to
    msg.push_back(12);                                      // data len
    uint8_t data[12] = {2, 0, 0, 0, 0x00, 0xCA, 0x9A, 0x3B, 0, 0, 0, 0};  // Transfer, 1e9 LE
    msg.insert(msg.end(), data, data + 12);

    // decode
    {
        TxPreview p = sol.decodeForDisplay(msg, *net);
        bool transfer = false, amt = false, toOk = false;
        for (auto& r : p.rows) {
            if (r.label == "Type" && r.value == "Transfer") transfer = true;
            if (r.label == "Amount" && r.value == "1 SOL") amt = true;
            if (r.label == "To" && r.value == base58Encode(to.data(), 32)) toOk = true;
        }
        ok("decode: Transfer / 1 SOL / correct To, not blind", transfer && amt && toOk && !p.blindSign);
    }

    // sign whole message + verify + encode
    {
        auto items = sol.buildSigningPayload(msg, *net, path);
        ok("one item, ed25519, not prehashed",
           items.size() == 1 && items[0].curve == Curve::Ed25519 && !items[0].prehashed &&
           items[0].payload == msg);
        Signature sig;
        bool signed_ok = w.sign(path, Curve::Ed25519, items[0].payload.data(), items[0].payload.size(),
                                false, sig);
        bool verifies = signed_ok && sig.size() == 64 &&
                        ed25519_sign_open(msg.data(), msg.size(), from.data(), sig.data()) == 0;
        ok("sign + ed25519 verify over message", verifies);

        Bytes tx = sol.encodeSigned(msg, {sig}, *net);
        bool wire = tx.size() == 1 + 64 + msg.size() && tx[0] == 1 &&
                    std::equal(msg.begin(), msg.end(), tx.begin() + 1 + 64);
        ok("encodeSigned wire format (count|sig|message)", wire);
    }

    std::printf("\n%s (%d failure%s)\n", g_fails ? "SOLANA CHAIN TESTS FAILED" : "ALL SOLANA CHAIN TESTS PASS",
                g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
