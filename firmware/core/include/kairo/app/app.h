#pragma once

namespace kairo {

class AppContext;

// IApp — a foreground application that runs on ITS OWN thread.
//
// Unlike a plugin/screen (which runs cooperatively on the GUI thread), an app's
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
};

} // namespace kairo
