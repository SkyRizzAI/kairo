// Host unit test — WalletController (Plan 94): onboarding/unlock + multi-wallet over the vault.

#include "nema/wallet/wallet_controller.h"
#include "nema/wallet/wallet_vault.h"
#include "nema/wallet/chains/evm.h"
#include "nema/wallet/chains/solana.h"
#include "nema/wallet/chains/bitcoin.h"

#include <cstdio>
#include <map>

extern "C" {
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
const char* M1 = "abandon abandon abandon abandon abandon abandon abandon "
                 "abandon abandon abandon abandon about";
const char* M2 = "legal winner thank year wave sausage worth useful legal winner thank yellow";
}  // namespace

int main() {
    MemKv kv;
    WalletVault vault(kv);
    EvmChain evm; SolanaChain sol; BitcoinChain btc;
    WalletService svc(vault);
    svc.registerChain(evm); svc.registerChain(sol); svc.registerChain(btc);
    WalletController ctl(vault, svc);

    ok("starts NoWallet", ctl.state() == WalletState::NoWallet);

    std::string m;
    ok("generate + validate", WalletController::generateMnemonic(false, m) && WalletController::validateMnemonic(m));

    ok("create first wallet → Unlocked", ctl.createFirstWallet(M1, "1234") && ctl.state() == WalletState::Unlocked);

    auto accts = ctl.accounts(WalletController::defaultNetworks());
    bool eth = false, bc = false, solOk = false;
    for (auto& a : accts) {
        if (a.networkId == "eth-mainnet" && a.address == "0x9858effd232b4033e47d90003d41ec34ecaeda94") eth = true;
        if (a.networkId == "btc-mainnet" && a.address == "bc1qcr8te4kr609gcawutmrza0j4xv80jy8z306fyu") bc = true;
        if (a.networkId == "sol-mainnet" && a.address.size() >= 32) solOk = true;
    }
    ok("active wallet accounts ETH+BTC+SOL", accts.size() == 3 && eth && bc && solOk);

    // Multiple wallets
    ok("add a 2nd wallet", ctl.addWallet(M2) && ctl.wallets().size() == 2);
    std::string id1 = ctl.wallets()[0].id;
    auto a2 = ctl.accounts(WalletController::defaultNetworks());
    ok("2nd wallet has a different ETH address", !a2.empty() && a2[0].address != "0x9858effd232b4033e47d90003d41ec34ecaeda94");
    ok("switch back to wallet 1", ctl.selectWallet(id1) &&
       ctl.accounts(WalletController::defaultNetworks())[0].address == "0x9858effd232b4033e47d90003d41ec34ecaeda94");

    // Multi-account within a wallet (one seed → many accounts, BIP44 index)
    auto acc0 = ctl.accounts(WalletController::defaultNetworks(), 0);
    ok("default 1 account", ctl.accountCount() == 1);
    ok("add account → 2", ctl.addAccount() && ctl.accountCount() == 2);
    auto acc1 = ctl.accounts(WalletController::defaultNetworks(), 1);
    ok("account index 1 ETH differs from index 0",
       !acc0.empty() && !acc1.empty() && acc0[0].address != acc1[0].address);
    std::string pk;
    ok("export private key (64 hex)", ctl.exportPrivateKey(0, "eth-mainnet", pk) && pk.size() == 64);
    ok("account count persists in vault index", vault.activeAccountCount() == 2);

    ctl.lock();
    ok("lock → Locked, no accounts", ctl.state() == WalletState::Locked && ctl.accounts(WalletController::defaultNetworks()).empty());
    ok("wrong PIN stays Locked", !ctl.unlock("0000") && ctl.state() == WalletState::Locked);
    ok("right PIN → Unlocked", ctl.unlock("1234") && ctl.state() == WalletState::Unlocked);

    ctl.wipe();
    ok("wipe → NoWallet", ctl.state() == WalletState::NoWallet);

    std::printf("\n%s (%d failure%s)\n", g_fails ? "WALLET CONTROLLER TESTS FAILED" : "ALL WALLET CONTROLLER TESTS PASS",
                g_fails, g_fails == 1 ? "" : "s");
    return g_fails ? 1 : 0;
}
