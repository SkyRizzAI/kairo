#pragma once
#include <cstdint>

namespace kairo {

class AppContext;

// IApp — a foreground application that runs on ITS OWN thread.
//
// Unlike a screen (which runs cooperatively on the GUI thread), an app's
// run() executes on a dedicated Nema thread. It MAY block (download, file I/O,
// long compute) without freezing the UI — the GUI thread keeps compositing the
// last frame the app presented, and input is delivered via the app's mailbox.
//
// run() must loop until ctx.shouldExit() and return promptly when it flips.
struct IApp {
    virtual ~IApp() = default;
    virtual const char* id()   const = 0;
    virtual const char* name() const = 0;
    virtual void run(AppContext& ctx) = 0;

    // false (default) → system status bar is shown above the app, and the app's
    // frame is composited below it. true → app owns the whole 264×176 screen.
    virtual bool fullscreen() const { return false; }

    // Stack size for this app's dedicated Nema thread. Override for apps that
    // need deep call stacks (e.g. JS apps running QuickJS, which recurses
    // deeply during evaluation). Default 8 KB fits all native C++ apps.
    virtual uint32_t stackBytes() const { return 8192; }
};

} // namespace kairo
