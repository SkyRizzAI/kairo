#include "kairo/services/profile_service.h"
#include "kairo/crypto/sha256.h"

namespace kairo {

static constexpr const char* NS         = "profile";
static constexpr const char* KEY_USER   = "user";
static constexpr const char* KEY_DEVICE = "device";
static constexpr const char* KEY_SALT   = "pwsalt";
static constexpr const char* KEY_HASH   = "pwhash";

void ProfileService::init(IConfigStore& cfg) {
    cfg_ = &cfg;

    bool hasUser = cfg.getString(NS, KEY_USER,   user_);
    bool hasDev  = cfg.getString(NS, KEY_DEVICE, device_);
    cfg.getString(NS, KEY_SALT, salt_);
    cfg.getString(NS, KEY_HASH, hash_);

    // Seed defaults on first boot (keys absent from NVS); never overwrite
    // values the user has already changed.
    if (!hasUser) { user_   = "Kairor";   cfg.setString(NS, KEY_USER,   user_); }
    if (!hasDev)  { device_ = "My Kairo"; cfg.setString(NS, KEY_DEVICE, device_); }
}

void ProfileService::setUserName(const std::string& n) {
    user_ = n;
    if (cfg_) cfg_->setString(NS, KEY_USER, n);
}

void ProfileService::setDeviceName(const std::string& n) {
    device_ = n;
    if (cfg_) cfg_->setString(NS, KEY_DEVICE, n);
}

void ProfileService::setPassword(const std::string& plain) {
    salt_ = randomHexSalt(16);                // 32-char hex salt
    hash_ = hexSha256(salt_ + plain);
    if (cfg_) {
        cfg_->setString(NS, KEY_SALT, salt_);
        cfg_->setString(NS, KEY_HASH, hash_);
    }
}

void ProfileService::clearPassword() {
    salt_.clear();
    hash_.clear();
    if (cfg_) {
        cfg_->remove(NS, KEY_SALT);
        cfg_->remove(NS, KEY_HASH);
    }
}

bool ProfileService::verifyPassword(const std::string& input) const {
    if (!hasPassword()) return false;
    std::string expected = hexSha256(salt_ + input);
    if (expected.size() != hash_.size()) return false;
    // Constant-time comparison: don't short-circuit on the first differing byte.
    uint8_t diff = 0;
    for (size_t i = 0; i < expected.size(); i++)
        diff |= (uint8_t)(expected[i] ^ hash_[i]);
    return diff == 0;
}

} // namespace kairo
