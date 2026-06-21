#include "nema/services/remote_auth.h"
#include "nema/config/config_store.h"
#include "nema/crypto/sha256.h"
#include <cstdio>
#include <cstring>

namespace nema {

static const char* NS = "remote";

bool RemoteAuthStore::enabled() const {
    if (!cfg_) return true;
    return cfg_->getIntOr(NS, "enabled", 1) != 0;
}
void RemoteAuthStore::setEnabled(bool on) {
    if (cfg_) cfg_->setInt(NS, "enabled", on ? 1 : 0);
}

bool RemoteAuthStore::hasPassword() const {
    if (!cfg_) return false;
    return !cfg_->getString(NS, "pwhash", "").empty();
}

void RemoteAuthStore::setPassword(const std::string& pw) {
    if (!cfg_) return;
    if (pw.empty()) { clearPassword(); return; }
    std::string salt = randomHexSalt(8);
    cfg_->setString(NS, "pwsalt", salt);
    cfg_->setString(NS, "pwhash", hexSha256(salt + pw));
    revokeAllTokens();   // password change invalidates existing logins
}

void RemoteAuthStore::clearPassword() {
    if (!cfg_) return;
    cfg_->remove(NS, "pwsalt");
    cfg_->remove(NS, "pwhash");
    revokeAllTokens();
}

std::string RemoteAuthStore::salt() const {
    return cfg_ ? cfg_->getString(NS, "pwsalt", "") : std::string();
}

bool RemoteAuthStore::verify(const std::string& nonce, const std::string& response) const {
    if (!cfg_) return false;
    std::string pwhash = cfg_->getString(NS, "pwhash", "");
    if (pwhash.empty()) return false;
    std::string expected = hexSha256(pwhash + nonce);
    // constant-time compare
    if (expected.size() != response.size()) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < expected.size(); i++) diff |= (uint8_t)(expected[i] ^ response[i]);
    return diff == 0;
}

std::string RemoteAuthStore::issueToken() {
    if (!cfg_) return "";
    std::string tok = randomHexSalt(16);
    int cnt = (int)cfg_->getIntOr(NS, "tokcnt", 0);
    if (cnt >= MAX_TOKENS) cnt = MAX_TOKENS - 1;   // evict oldest (shift down)
    if (cnt == MAX_TOKENS - 1) {
        for (int i = 0; i < MAX_TOKENS - 1; i++) {
            char a[16], b[16];
            std::snprintf(a, sizeof(a), "tok%d", i);
            std::snprintf(b, sizeof(b), "tok%d", i + 1);
            cfg_->setString(NS, a, cfg_->getString(NS, b, ""));
        }
    }
    char k[16];
    std::snprintf(k, sizeof(k), "tok%d", cnt);
    cfg_->setString(NS, k, tok);
    cfg_->setInt(NS, "tokcnt", cnt + 1);
    return tok;
}

bool RemoteAuthStore::validateToken(const std::string& token) const {
    if (!cfg_ || token.empty()) return false;
    int cnt = (int)cfg_->getIntOr(NS, "tokcnt", 0);
    if (cnt > MAX_TOKENS) cnt = MAX_TOKENS;
    for (int i = 0; i < cnt; i++) {
        char k[16];
        std::snprintf(k, sizeof(k), "tok%d", i);
        if (cfg_->getString(NS, k, "") == token) return true;
    }
    return false;
}

void RemoteAuthStore::revokeAllTokens() {
    if (!cfg_) return;
    for (int i = 0; i < MAX_TOKENS; i++) {
        char k[16];
        std::snprintf(k, sizeof(k), "tok%d", i);
        cfg_->remove(NS, k);
    }
    cfg_->setInt(NS, "tokcnt", 0);
}

size_t RemoteAuthStore::tokenCount() const {
    return cfg_ ? (size_t)cfg_->getIntOr(NS, "tokcnt", 0) : 0;
}

bool RemoteAuthStore::isPrivileged(plp::Channel ch) {
    switch (ch) {
        case plp::Channel::Cli:
        case plp::Channel::File:
        case plp::Channel::Ota:
        case plp::Channel::Ext:
        case plp::Channel::System:
        case plp::Channel::Input:
            return true;
        default:   // Control, Screen, Log, Event — observation tier
            return false;
    }
}

} // namespace nema
