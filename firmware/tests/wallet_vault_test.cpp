// Host unit test — WalletVault (Plan 94): multiple wallets under one PIN.

#include "nema/wallet/wallet_vault.h"
#include "nema/wallet/wallet_service.h"
#include "nema/wallet/network_registry.h"
#include "nema/wallet/chains/evm.h"
#include "nema/wallet/soft_secure_element.h"

#include <cstdio>
#include <cstring>
#include <map>

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
struct MemKv : IKvStore {
    std::map<std::string, std::vector<uint8_t>> m;
    bool put(const char* k, const uint8_t* d, size_t n) override { m[k].assign(d, d + n); return true; }
    bool get(const char* k, std::vector<uint8_t>& o) override { auto it = m.find(k); if (it == m.end()) return false; o = it->second; return true; }
    bool has(const char* k) const override { return m.count(k) > 0; }
    void del(const char* k) override { m.erase(k); }
};

std::string ethAddr(IWalletBackend& be, EvmChain& evm, const NetworkParams& net) {
    PubKey pub;
    if (!be.publicKey(evm.pathFor(0, net), Curve::Secp256k1, pub)) return "";
    uint8_t p65[65]; ecdsa_uncompress_pubkey(&secp256k1, pub.data(), p65);
    uint8_t h[32]; keccak_256(p65 + 1, 64, h);
    char a[43]; a[0] = '0'; a[1] = 'x';
    for (int i = 0; i < 20; i++) std::snprintf(a + 2 + 2 * i, 3, "%02x", h[12 + i]);
    return std::string(a);
}
const char* M1 = "abandon abandon abandon abandon abandon abandon abandon "
                 "abandon abandon abandon abandon about";
const char* M2 = "legal winner thank year wave sausage worth useful legal winner thank yellow";
const char* ETH1 = "0x9858effd232b4033e47d90003d41ec34ecaeda94";
}  // namespace

int main() {
    MemKv kv;
    EvmChain evm;
    const NetworkParams* eth = NetworkRegistry::find("eth-mainnet");

    {
        WalletVault v(kv);
        ok("starts empty", !v.hasWallet());

        std::string id1;
        ok("createFirst (sets PIN)", v.createFirst(M1, "1234", id1) && v.unlocked());
        ok("1 wallet, active=Wallet1, addr=ETH1",
           v.wallets().size() == 1 && v.activeId() == id1 && ethAddr(v, evm, *eth) == ETH1);

        std::string id2;
        ok("addWallet (no PIN re-prompt)", v.addWallet(M2, id2) && v.wallets().size() == 2);
        std::string addr2 = ethAddr(v, evm, *eth);
        ok("active switched to Wallet2 (different address)", v.activeId() == id2 && addr2 != ETH1);
        ok("labels Wallet 1 / Wallet 2",
           v.wallets()[0].label == "Wallet 1" && v.wallets()[1].label == "Wallet 2");

        ok("select back to Wallet1 → ETH1", v.select(id1) && ethAddr(v, evm, *eth) == ETH1);

        v.lock();
        ok("locked", !v.unlocked() && v.hasWallet());
        ok("wrong PIN rejected", !v.unlock("9999"));
        ok("right PIN unlocks", v.unlock("1234") && ethAddr(v, evm, *eth) == ETH1);
    }

    // Persistence: a fresh vault over the same store sees both wallets.
    {
        WalletVault v2(kv);
        ok("reopened vault: 2 wallets, still locked", v2.hasWallet() && v2.wallets().size() == 2 && !v2.unlocked());
        ok("reopened unlock + addresses intact", v2.unlock("1234"));
        std::string id1 = v2.wallets()[0].id, id2 = v2.wallets()[1].id;
        ok("switch across reopen", v2.select(id1) && ethAddr(v2, evm, *eth) == ETH1);

        ok("remove Wallet2 → 1 left", v2.remove(id2) && v2.wallets().size() == 1);
        v2.wipe();
        ok("wipe → empty", !v2.hasWallet());
    }

    // ── Mode B: secure-element device-bound sealing (SoftSecureElement) ──
    // Same vault logic, but each PIN-encrypted seed is additionally wrapped by the
    // chip's device key. Proves the auto-selected SE path end-to-end without hardware.
    {
        MemKv kvB;
        nema::SoftSecureElement chipA;            // "device A" (fixed dev key)
        std::string id;
        {
            WalletVault v(kvB, &chipA);
            ok("SE vault kind = SecureElement", v.kind() == BackendKind::SecureElement);
            ok("SE createFirst", v.createFirst(M1, "1234", id) && v.unlocked());
            ok("SE address = ETH1 (HD unaffected)", ethAddr(v, evm, *eth) == ETH1);
            v.lock();
            ok("SE wrong PIN rejected", !v.unlock("9999"));
            ok("SE right PIN: unwrap+decrypt → ETH1", v.unlock("1234") && ethAddr(v, evm, *eth) == ETH1);
        }
        {  // device-binding: a DIFFERENT chip can't unwrap the stored seed
            uint8_t otherKey[32];
            for (int i = 0; i < 32; i++) otherKey[i] = (uint8_t)(0x42 ^ i);  // ≠ default 0xA0+i
            nema::SoftSecureElement chipB(otherKey);   // "device B"
            WalletVault v(kvB, &chipB);
            ok("SE device-binding: wrong chip can't unlock even with right PIN",
               v.hasWallet() && !v.unlock("1234"));
        }
        {  // same chip reopened → works again (persistence + correct unwrap)
            WalletVault v(kvB, &chipA);
            ok("SE same chip reopen unlocks", v.unlock("1234") && ethAddr(v, evm, *eth) == ETH1);
        }
        WalletVault soft(kv);
        ok("software vault kind = Software (⚠️)", soft.kind() == BackendKind::Software);
    }

    std::printf("\n%s (%d failure%s)\n", g_fails ? "WALLET VAULT TESTS FAILED" : "ALL WALLET VAULT TESTS PASS",
                g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
