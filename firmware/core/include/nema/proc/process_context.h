#pragma once
#include "nema/proc/stream.h"
#include <string>
#include <vector>

// Plan 54 — ProcessContext: the kernel surface a process sees.
// Runtime-agnostic: native C++, WASM (via WASI), and JS all map to this.
// Contains argv, cwd, env, stdin/stdout/stderr, exit mechanism, and Runtime access.
// UI apps additionally request a surface (Plan 55), which is NOT part of this class.

namespace nema {

class Runtime;

class ProcessContext {
public:
    virtual ~ProcessContext() = default;

    // ── argv ──────────────────────────────────────────────────────────
    // argv[0] = name used to invoke (Unix convention).
    // Flag parsing (commander/clap/getopt) lives in userspace/SDK, not kernel.
    virtual const std::vector<std::string>& args() const = 0;

    // ── cwd / env ─────────────────────────────────────────────────────
    // Inherited from the launching CliSession (cli_service.h:52).
    virtual const std::string& cwd() const = 0;
    virtual const char*        env(const char* key) const = 0;   // nullptr if absent

    // ── stdio (fd 0/1/2) ──────────────────────────────────────────────
    virtual IInputStream&  in()  = 0;     // stdin  (pipe read-end, or null stream)
    virtual IOutputStream& out() = 0;     // stdout (→ CliSession.out or pipe write-end)
    virtual IOutputStream& err() = 0;     // stderr

    // ── exit ──────────────────────────────────────────────────────────
    // requestExit(code) signals the process to terminate. run() should return
    // promptly after shouldExit() becomes true (same pattern as AppContext).
    virtual void requestExit(int code = 0) = 0;
    virtual bool shouldExit() const = 0;
    virtual int  exitCode()   const = 0;

    // ── runtime ───────────────────────────────────────────────────────
    // Full access to TaskRunner, clock, log(), events, capabilities, etc.
    virtual Runtime& runtime() = 0;
};

// Default (empty) environment for processes without a CliSession context.
class DefaultProcessEnv {
public:
    const std::string& cwd() const { return cwd_; }
    const char*        env(const char*) const { return nullptr; }

private:
    std::string cwd_ = "/";
};

} // namespace nema
