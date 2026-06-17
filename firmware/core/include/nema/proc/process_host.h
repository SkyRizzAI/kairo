#pragma once
#include "nema/proc/process_context.h"
#include "nema/proc/stream.h"
#include "nema/thread.h"
#include <atomic>
#include <string>
#include <vector>

// Plan 54 — ProcessHost: spawns an app on its own thread with a ProcessContext.
// Extracted from AppHost's thread-management role. AppHost still handles the
// UI bridge (canvas/present/input mailbox → IScreen for ViewDispatcher).

namespace nema {

class Runtime;
struct IApp;

// ── ProcessSpec — assembled by the shell at launch time ───────────────────

struct ProcessSpec {
    std::vector<std::string> argv;
    std::string              cwd = "/";
    IInputStream*            stdin_  = nullptr;   // pipe read-end or null
    IOutputStream*           stdout_ = nullptr;   // CliSession.out or pipe write-end
    IOutputStream*           stderr_ = nullptr;   // CliSession.out
};

// ── ProcessHost — thread + ProcessContext for one app ─────────────────────

class ProcessHost : public ProcessContext {
public:
    ProcessHost(Runtime& rt, IApp& app, ProcessSpec spec);
    ~ProcessHost();

    // Spawn the app thread. Calls app.run(*this) on the new thread.
    void start();

    // Has the app thread exited? (poll from GUI / manager loop)
    bool finished() const;

    // Block until the app thread exits, return the exit code.
    int  join();

    // ── ProcessContext impl ────────────────────────────────────────────
    const std::vector<std::string>& args() const override { return spec_.argv; }
    const std::string&              cwd()  const override { return spec_.cwd; }
    const char*                     env(const char* key) const override;

    IInputStream&  in()  override;
    IOutputStream& out() override;
    IOutputStream& err() override;

    void    requestExit(int code = 0) override;
    bool    shouldExit() const override;
    int     exitCode()   const override;
    Runtime& runtime()          override;

private:
    static void threadEntry(void* self);

    Runtime&      rt_;
    IApp&         app_;
    ProcessSpec   spec_;

    Thread        thread_;
    std::atomic<int>  exitCode_{0};
    std::atomic<bool> exitReq_{false};

    // Default streams when caller doesn't supply them
    NullInputStream     nullIn_;
    StringOutputStream  nullOut_;
};

} // namespace nema
