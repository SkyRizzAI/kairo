#pragma once
#include "nema/ui/screen.h"
#include "nema/ui/view_dispatcher.h"

namespace nema {

// Plan 70: AppWindow — a screen that wraps an app with its own internal
// navigation stack. The system pushes AppWindow onto its own ViewDispatcher;
// inside the AppWindow, the app manages its own ViewDispatcher with whatever
// internal screens it needs (navigate/replace/goBack).
//
// AppWindow delegates rendering to its internal ViewDispatcher's active screen.
// When the system pauses/resumes the app, AppWindow forwards the lifecycle
// to the internal stack.
//
// For apps that don't need multi-screen navigation, AppContext can still be
// used directly (AppHost remains available).

class AppWindow : public IScreen {
public:
    explicit AppWindow(const char* appName);

    // IScreen — fullscreen app (no system status bar by default)
    ScreenMode mode() const override { return ScreenMode::Fullscreen; }

    void onResume() override;
    void onPause()  override;
    void onStop()   override;
    bool onBackPressed() override;   // consumes Back if internal VD can goBack

    void onAction(input::Action a) override;
    void onCode(input::Code c) override;
    void onPointer(const input::PointerEvent& e) override;

    void draw(Canvas& canvas) override;
    void tick(uint64_t nowMs) override;

    // Per-app navigation stack — the app pushes its own screens here.
    ViewDispatcher& stack() { return vd_; }

    const char* appName() const { return appName_.c_str(); }

private:
    std::string     appName_;
    ViewDispatcher  vd_;             // app's own screen stack
};

} // namespace nema
