#pragma once
#include "nema/input_event.h"
#include <cstdint>

namespace nema {

class Canvas;
class Runtime;

// AppContext — the only surface an app touches. Given to IApp::run() and lives
// on the app thread. All UI handoff is via present() (buffer) + nextInput()
// (mailbox) — the app NEVER touches the real Canvas or ViewDispatcher, so there
// is no cross-thread UI race by construction.
class AppContext {
public:
    virtual ~AppContext() = default;

    // The app draws here (a Canvas over an in-RAM buffer). Same API as the real
    // screen — existing draw code works unchanged.
    virtual Canvas& canvas() = 0;

    // Publish the freshly drawn frame to the GUI thread (copies buffer + asks
    // for a redraw). Cheap; call after each draw.
    virtual void present() = 0;

    // Pop one input event from the app's mailbox. Non-blocking (false if none).
    virtual bool nextInput(InputEvent& out) = 0;
    // Block up to timeoutMs for input (false on timeout). Use to idle the app
    // thread without busy-spinning.
    virtual bool waitInput(InputEvent& out, uint32_t timeoutMs) = 0;

    // App asks to exit; run() should return soon after seeing shouldExit().
    virtual void requestExit() = 0;
    virtual bool shouldExit() const = 0;

    // Full runtime access (TaskRunner for heavy work, clock, log, events).
    virtual Runtime& runtime() = 0;
};

} // namespace nema
