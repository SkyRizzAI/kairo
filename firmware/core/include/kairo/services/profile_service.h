#pragma once
#include "kairo/service.h"
#include "kairo/config/config_store.h"
#include <string>

namespace kairo {

// ProfileService — single owner identity for the device (Plan 40).
//
// Stores three fields in NVS namespace "profile": userName, deviceName, and an
// optional password (kept as salt + SHA-256 hash — never plaintext).
//
// First boot seeds defaults ("Kairor" / "My Kairo" / no password). Subsequent
// boots never overwrite persisted values — changes by the user survive reboots.
//
// There is intentionally NO enforcement: no lock screen gates, no auth
// gating on remote/CLI. This service only stores identity + exposes a
// verifyPassword() API for apps/screens that want to check a PIN.
class ProfileService : public IService {
public:
    const char* name() const override { return "profile"; }
    void start() override {}
    void stop()  override {}

    // Must be called once before any get/set. Loads persisted values and seeds
    // defaults if the NVS namespace is empty (first boot).
    void init(IConfigStore& cfg);

    const std::string& userName()   const { return user_; }
    const std::string& deviceName() const { return device_; }

    void setUserName  (const std::string& n);
    void setDeviceName(const std::string& n);

    bool hasPassword() const { return !hash_.empty(); }
    void setPassword  (const std::string& plain);  // generates fresh salt
    void clearPassword();
    // Returns false if password is not set. Uses constant-time comparison.
    bool verifyPassword(const std::string& input) const;

private:
    IConfigStore* cfg_ = nullptr;
    std::string   user_, device_, salt_, hash_;
};

} // namespace kairo
