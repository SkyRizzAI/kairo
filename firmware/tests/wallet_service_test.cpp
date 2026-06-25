// Host unit test — WalletService (Plan 94, Fase 3).
//
// Orchestration across the three axes: derive addresses for all chains via one service,
// preview + sign through the consent gate, and prove the EVM signature embedded in the
// returned tx is valid for the account. Also checks fail-closed on consent rejection.

#include "nema/wallet/wallet_service.h"
#include "nema/wallet/backends/nvs_backend.h"
#include "nema/wallet/network_registry.h"
#include "nema/wallet/chains/evm.h"
#include "nema/wallet/chains/solana.h"
#include "nema/wallet/chains/bitcoin.h"
#include "nema/wallet/chains/rlp.h"

#include <cstdio>
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
Bytes pad32(const Bytes& b) { Bytes o(32 - b.size(), 0); o.insert(o.end(), b.begin(), b.end()); return o; }
}  // namespace

int main() {
    MemStore store;
    NvsBackend backend(store);
    backend.create(
        "abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon about", "", "1234");

    EvmChain evm; SolanaChain sol; BitcoinChain btc;
    WalletService ws(backend);
    ws.registerChain(evm);
    ws.registerChain(sol);
    ws.registerChain(btc);

    ok("ready + Software backend indicator", ws.ready() && ws.activeBackendKind() == BackendKind::Software);

    // Addresses for all three chains via the one service.
    std::string eth, bc, solAddr;
    ok("derive ETH", ws.deriveAddress("eth-mainnet", 0, eth) &&
       eth == "0x9858effd232b4033e47d90003d41ec34ecaeda94");
    ok("derive BTC", ws.deriveAddress("btc-mainnet", 0, bc) &&
       bc == "bc1qcr8te4kr609gcawutmrza0j4xv80jy8z306fyu");
    ok("derive SOL (non-empty)", ws.deriveAddress("sol-mainnet", 0, solAddr) && solAddr.size() >= 32);
    ok("unknown network rejected", !ws.deriveAddress("nope", 0, eth));
    eth = "0x9858effd232b4033e47d90003d41ec34ecaeda94";  // restore (above overwrote on fail path)

    // Build a legacy EIP-155 tx (chainId 1) for our account.
    auto S = [](const Bytes& b) { return rlp::encodeString(b); };
    Bytes to(20, 0x42);
    Bytes rawTx = rlp::encodeList({
        S(Bytes{0x01}), S(Bytes{0x04, 0xa8, 0x17, 0xc8, 0x00}), S(Bytes{0x52, 0x08}),
        S(to), S(Bytes{0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64, 0x00, 0x00}), S(Bytes{}),
        S(Bytes{0x01}), S(Bytes{}), S(Bytes{}),
    });

    // preview
    {
        TxPreview p;
        bool toRow = false;
        if (ws.previewTransaction("eth-mainnet", rawTx, p))
            for (auto& r : p.rows) if (r.label == "To") toRow = true;
        ok("previewTransaction shows To", toRow);
    }

    // fail-closed: rejected consent → no signature
    {
        Bytes sig;
        bool r = ws.signTransaction("eth-mainnet", 0, rawTx, [](const TxPreview&) { return false; }, sig);
        ok("consent reject → fail-closed", !r && sig.empty());
    }

    // approved → valid signed tx; verify the embedded signature for our pubkey
    {
        Bytes signedTx;
        bool r = ws.signTransaction("eth-mainnet", 0, rawTx, [](const TxPreview&) { return true; }, signedTx);
        ok("approved → non-empty signed tx", r && !signedTx.empty());

        std::vector<Bytes> items;
        rlp::decodeList(signedTx, items);
        bool verified = false;
        if (items.size() == 9) {
            Bytes rr = pad32(items[7]), ss = pad32(items[8]);
            Bytes rs = rr; rs.insert(rs.end(), ss.begin(), ss.end());
            uint8_t digest[32]; keccak_256(rawTx.data(), rawTx.size(), digest);
            PubKey pub; backend.publicKey(evm.pathFor(0, *NetworkRegistry::find("eth-mainnet")),
                                          Curve::Secp256k1, pub);
            verified = ecdsa_verify_digest(&secp256k1, pub.data(), rs.data(), digest) == 0;
        }
        ok("embedded EVM signature verifies for account", verified);
    }

    // Solana sign through the service
    {
        // minimal transfer message (from our pubkey)
        PubKey from; ws.deriveAddress("sol-mainnet", 0, solAddr);
        backend.publicKey(sol.pathFor(0, *NetworkRegistry::find("sol-mainnet")), Curve::Ed25519, from);
        Bytes msg; msg.push_back(1); msg.push_back(0); msg.push_back(1); msg.push_back(3);
        msg.insert(msg.end(), from.begin(), from.end());
        Bytes z(32, 0x02); msg.insert(msg.end(), z.begin(), z.end());
        Bytes sys(32, 0x00); msg.insert(msg.end(), sys.begin(), sys.end());
        Bytes bh(32, 0x01); msg.insert(msg.end(), bh.begin(), bh.end());
        msg.push_back(1); msg.push_back(2); msg.push_back(2); msg.push_back(0); msg.push_back(1);
        msg.push_back(12);
        uint8_t d[12] = {2, 0, 0, 0, 0x00, 0xCA, 0x9A, 0x3B, 0, 0, 0, 0};
        msg.insert(msg.end(), d, d + 12);
        Bytes signedTx;
        bool r = ws.signTransaction("sol-mainnet", 0, msg, [](const TxPreview&) { return true; }, signedTx);
        ok("Solana sign via service → wire tx", r && signedTx.size() == 1 + 64 + msg.size());
    }

    std::printf("\n%s (%d failure%s)\n", g_fails ? "WALLET SERVICE TESTS FAILED" : "ALL WALLET SERVICE TESTS PASS",
                g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
