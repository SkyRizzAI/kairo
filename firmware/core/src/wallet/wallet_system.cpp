#include "nema/wallet/wallet_system.h"

#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/service/service_container.h"
#include "nema/system/capabilities.h"
#include "nema/system/capability_registry.h"
#include "nema/config/config_store.h"

#include <cstdio>
#include <string>
#include <utility>

namespace nema::wallet {

namespace {
// Wallet blob store backed by NVS (ADR 0022). Chosen over the LittleFS /system
// partition because that partition is re-flashed by a full `idf.py flash` (it also
// carries the read-only asset image), which used to wipe the wallet on every
// firmware update. The `nvs` partition is NOT in the flash image, so wallets now
// survive firmware updates and a LittleFS reformat. NvsConfigStore commits on every
// setString, so writes are durable across a restart immediately.
//
// IConfigStore is string-only, so binary blobs are hex-encoded. NVS keys are capped
// at 15 chars, so every key is hashed to 8 hex chars (djb2) — collisions across the
// handful of wallet keys are astronomically unlikely.
class ConfigKvStore : public IKvStore {
public:
    explicit ConfigKvStore(IConfigStore& cfg) : cfg_(cfg) {}

    bool put(const char* key, const uint8_t* d, size_t n) override {
        cfg_.setString(kNs, k(key).c_str(), toHex(d, n));
        return true;
    }
    bool get(const char* key, std::vector<uint8_t>& o) override {
        std::string hex;
        if (!cfg_.getString(kNs, k(key).c_str(), hex)) return false;
        return fromHex(hex, o);
    }
    bool has(const char* key) const override {
        std::string hex;
        return cfg_.getString(kNs, k(key).c_str(), hex);
    }
    void del(const char* key) override { cfg_.remove(kNs, k(key).c_str()); }

private:
    static constexpr const char* kNs = "wallet";
    IConfigStore& cfg_;

    static std::string k(const char* key) {                 // → 8-hex NVS key (≤15)
        uint32_t h = 5381;
        for (const char* p = key; *p; ++p) h = ((h << 5) + h) + (uint8_t)*p;
        char b[9]; std::snprintf(b, sizeof(b), "%08x", (unsigned)h);
        return b;
    }
    static std::string toHex(const uint8_t* d, size_t n) {
        static const char* hx = "0123456789abcdef";
        std::string s; s.reserve(n * 2);
        for (size_t i = 0; i < n; i++) { s.push_back(hx[d[i] >> 4]); s.push_back(hx[d[i] & 15]); }
        return s;
    }
    static bool fromHex(const std::string& s, std::vector<uint8_t>& o) {
        if (s.size() & 1) return false;
        auto nib = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        o.clear(); o.reserve(s.size() / 2);
        for (size_t i = 0; i < s.size(); i += 2) {
            int hi = nib(s[i]), lo = nib(s[i + 1]);
            if (hi < 0 || lo < 0) return false;
            o.push_back((uint8_t)((hi << 4) | lo));
        }
        return true;
    }
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
    // Encrypted seeds + index live in NVS (survives `idf.py flash` + LittleFS reformat;
    // see ADR 0022). The seed is still PIN-encrypted and, with a secure element,
    // additionally device-bound — NVS just holds the ciphertext.
    static ConfigKvStore kv(rt.config());

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
