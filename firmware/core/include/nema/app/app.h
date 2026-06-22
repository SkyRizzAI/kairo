#pragma once
#include <cstdint>

namespace nema {

class AppContext;
class ProcessContext;

// IApp — a foreground application that runs on ITS OWN thread.
//
// Unlike a screen (which runs cooperatively on the GUI thread), an app's
// run() executes on a dedicated Nema thread. It MAY block (download, file I/O,
// long compute) without freezing the UI — the GUI thread keeps compositing the
// last frame the app presented, and input is delivered via the app's mailbox.
//
// run() must loop until ctx.shouldExit() and return promptly when it flips.
//
// Plan 54: Two entry points.
//   run(AppContext&)       — UI app (canvas, present, nextInput, status bar).
//   runProcess(ProcessContext&) — headless app (args, stdin/stdout/stderr, exit).
// Headless apps override runProcess(); UI apps override run().
struct IApp {
    virtual ~IApp() = default;
    virtual const char* id()   const = 0;
    virtual const char* name() const = 0;
    virtual void run(AppContext& ctx) = 0;

    // Headless entry point (Plan 54). Called by the shell's `run` command.
    // Default: no-op (app is UI-only and can't run headless).
    virtual void runProcess(ProcessContext&) {}

    // false (default) → system status bar is shown above the app, and the app's
    // frame is composited below it. true → app owns the whole 264×176 screen.
    virtual bool fullscreen() const { return false; }

    // Stack size for this app's dedicated Nema thread. Override for apps that
    // need deep call stacks (e.g. JS apps running QuickJS, which recurses
    // deeply during evaluation). Default 8 KB fits all native C++ apps.
    virtual uint32_t stackBytes() const { return 8192; }

    // Launcher group label. Default "Apps". System tools return "System".
    virtual const char* category() const { return "Apps"; }

    // Escalated kill (Plan 87 Fase 6). Called after requestExit() if the app
    // has not terminated within the watchdog grace period. WASM: traps the VM
    // at the next function-call boundary via m3_Yield. JS: similar.
    // Default: no-op (native C++ apps are expected to honour shouldExit()).
    virtual void requestAbort() {}
};

} // namespace nema
