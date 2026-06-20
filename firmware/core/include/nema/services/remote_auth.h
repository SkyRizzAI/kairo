#pragma once
#include "nema/link/plp_codec.h"
#include <string>
#include <cstdint>

namespace nema {

struct IConfigStore;

// RemoteAuthStore — authorization policy for the PLP remote layer (Plan 74).
//
// Tiers: OBSERVATION channels (Screen/Log/Event/Control) are always allowed once
// the link handshake completes; PRIVILEGED channels (Cli/File/Ota/Ext/System/
// Input) can change state or run code and need an authorized session.
//
// Auth is a salted challenge-response so the password never travels in cleartext
// and is stored only as a hash:
//   stored  pwhash = sha256(salt + password)
//   device  challenge = {salt, nonce}
//   host    response  = sha256(sha256(salt + pw_entered) + nonce)
//   device  expected  = sha256(pwhash + nonce)   → constant-time compare
//
// Backward-compatible default: with NO password set, privileged channels stay
// open (today's behaviour — USB CLI keeps working). Setting a password turns on
// gating across all transports. Persisted via IConfigStore (namespace "remote").
class RemoteAuthStore {
public:
    void init(IConfigStore& cfg) { cfg_ = &cfg; }

    // Master switch (Settings -> Remote). Default ON.
    bool enabled() const;
    void setEnabled(bool on);

    // Password.
    bool        hasPassword() const;
    void        setPassword(const std::string& pw);
    void        clearPassword();
    std::string salt() const;                                   // for the challenge
    bool        verify(const std::string& nonce, const std::string& response) const;

    // Session tokens (a "remember me" so a host reconnects without re-entering the
    // password). Issued after a successful password auth; revocable.
    std::string issueToken();
    bool        validateToken(const std::string& token) const;
    void        revokeAllTokens();
    size_t      tokenCount() const;

    // Channel tier. Privileged channels require an authorized session when a
    // password is set.
    static bool isPrivileged(plp::Channel ch);

private:
    IConfigStore* cfg_ = nullptr;
    static constexpr int MAX_TOKENS = 4;
};

} // namespace nema
