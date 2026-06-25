#include "nema/wallet/wallet_system.h"

#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/service/service_container.h"
#include "nema/system/capabilities.h"
#include "nema/system/capability_registry.h"
#include "nema/fs/app_storage.h"

#include <utility>

namespace nema::wallet {

namespace {
// Multi-key blob store backed by a fixed system bundle's internal-flash storage.
class AppStorageKvStore : public IKvStore {
public:
    explicit AppStorageKvStore(AppStorage st) : st_(std::move(st)) {}
    bool put(const char* key, const uint8_t* d, size_t n) override { return st_.write(key, d, n); }
    bool get(const char* key, std::vector<uint8_t>& o) override { return st_.read(key, o); }
    bool has(const char* key) const override { return const_cast<AppStorage&>(st_).exists(key); }
    void del(const char* key) override { st_.remove(key); }

private:
    AppStorage st_;
};
}  // namespace

WalletSystem::WalletSystem(IKvStore& store, ISecureElement* se)
    : vault_(store, se), svc_(vault_), ctl_(vault_, svc_) {
    svc_.registerChain(evm_);
    svc_.registerChain(sol_);
    svc_.registerChain(btc_);
}

void WalletSystem::registerInto(Runtime& rt) {
    rt.container().registerAs<WalletService>(&svc_);
    rt.container().registerAs<WalletConsentService>(&consent_);
    rt.container().registerAs<WalletController>(&ctl_);
}

WalletSystem& bootWalletSystem(Runtime& rt) {
    // Seeds live under a dedicated system bundle, internal flash only (never SD).
    static AppStorageKvStore kv(AppStorage("com.palanu.wallet", rt.fs(), rt.config(), /*critical*/ true));

    // Smart backend selection: if the board registered a secure element that can do
    // device-bound sealing (SeFeature::SecureStore), use it to wrap the seed (mode B,
    // 🔒). Otherwise stay pure-software (mode C, ⚠️) — the PIN is still a real crypto
    // gate. No board branching: we ask the chip what it can do.
    // Capability-driven (CLAUDE.md: check capabilities, never board type). A board with
    // a real secure element registers its driver AND declares caps::Secure; a board
    // without one (e.g. the WASM simulator) does neither — so here we see no caps::Secure,
    // resolve no driver, and fall back to software/NVS (mode C). "SE first, else software."
    bool hasSecure = rt.capabilities().has(caps::Secure);
    ISecureElement* se = hasSecure ? rt.container().resolve<ISecureElement>() : nullptr;
    bool sealable = se && se->hasFeature(SeFeature::SecureStore);
    rt.log().info("WalletSystem", "backend",
                  {{"secureElement", se ? se->name() : "none"},
                   {"capability", hasSecure ? "secure.element" : "none"},
                   {"sealable", sealable ? "yes" : "no"},
                   {"mode", sealable ? "B (secure-element)" : "C (software)"}});

    static WalletSystem sys(kv, sealable ? se : nullptr);
    sys.registerInto(rt);
    return sys;
}

}  // namespace nema::wallet
