#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace nema {

class Runtime;
struct CliContext;

// CliService — the command REGISTRY (Plan 44). It is not a Unix shell (no kernel,
// no processes), but it is a real shell in the sense that commands run inside a
// per-connection CliSession with persistent state (history + working directory).
// Transport-agnostic: RemoteService routes each PLP CLI line into a session.
//
// Built to extend. add() is the single entry point, shared by core built-ins
// (registerCoreCliCommands), platform specializations (e.g. ESP32's live-heap
// `ram`), and future `.kapp` apps that register their own commands.
class CliService {
public:
    using Out     = std::function<void(const std::string&)>;  // emit one output line
    using Handler = std::function<void(CliContext& ctx)>;       // ctx = args + out + session

    struct Command {
        std::string name;
        std::string help;
        Handler     handler;
    };

    // Register a command, or REPLACE the one with the same name (specialize).
    void add(std::string name, std::string help, Handler handler);

    // Execute one line inside a session: parse → push history → dispatch with a
    // CliContext. cwd/history persist across calls for that session.
    void execute(const std::string& line, struct CliSession& session);
    // Convenience: run with just an output sink (a throwaway, stateless session).
    void execute(const std::string& line, const Out& out);

    const std::vector<Command>& commands() const { return cmds_; }

private:
    std::vector<Command> cmds_;
};

// Per-connection shell session (Plan 44): device-side state that persists across
// command lines for one connected client (USB / BLE / WASM cable). Each link
// connection gets its own — history and cwd are isolated per connection.
struct CliSession {
    uint32_t                 id    = 0;
    std::string              cwd   = "/";   // working directory (over the VFS)
    std::vector<std::string> history;       // device-side, capped ring
    CliService::Out          out;           // this connection's output sink
    bool                     alive = true;

    void pushHistory(const std::string& line);   // cap ~32, skips blanks/dupes
    void reset() { cwd = "/"; history.clear(); }  // on (re)connect
};

// Context handed to every command handler.
struct CliContext {
    const std::vector<std::string>& args;     // argv after the command name
    const CliService::Out&          out;      // == session.out
    CliSession&                     session;  // history + cwd
};

// CliSessionManager (Plan 45) — owns the live CLI sessions, keyed by a 1-byte
// session id carried in the CLI channel. Each id is an independent shell
// (separate cwd + history), so concurrent sessions (remote A, remote B, a future
// local TTY) never interfere. Pointers stay stable as sessions are added, so a
// CliSession& from get() remains valid.
class CliSessionManager {
public:
    CliSession& get(uint8_t sid);     // get-or-create the session for sid
    void        remove(uint8_t sid);
    void        clear();
    const std::vector<std::unique_ptr<CliSession>>& sessions() const { return sessions_; }

private:
    std::vector<std::unique_ptr<CliSession>> sessions_;
};

// Register the built-in commands (help, hwinfo, ram, caps, display, power,
// wlan/network, ble/bluetooth, whoami, profile, fs, plus the shell built-ins
// pwd/cd/history). They read everything through `rt`.
void registerCoreCliCommands(CliService& cli, Runtime& rt);

} // namespace nema
