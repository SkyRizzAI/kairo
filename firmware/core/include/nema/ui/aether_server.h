#pragma once
#include "nema/ui/display_server.h"
#include "nema/ui/ui_sdk.h"
#include <cstdint>

namespace nema {

struct IClock;
struct StyleTokens;

// AetherServer — the built-in 1-bit canvas renderer (the default display
// server). Composites the status bar + active screen/modal onto the Canvas and
// flushes, with an optional FPS/timing overlay. This is exactly the rendering
// that used to live inline in GuiService::renderOnce.
//
// Plan 50: exposes the "aether:ui" widget SDK (namespace + bindings).
class AetherServer : public IDisplayServer {
public:
    explicit AetherServer(IClock& clock) : clock_(clock) {}

    const char* name() const override { return "aether"; }

    // Plan 51 — AetherServer requires a pixel-addressable display.
    const char* const* requiredCaps() const override {
        static const char* kCaps[] = { "display", nullptr };
        return kCaps;
    }

    void renderFrame(Canvas& c, ViewDispatcher& views, const StatusBarData& status) override;

    // UI SDK (Plan 50) — Aether exposes its own widget namespace.
    const UiSdkDescriptor* uiSdk() const override;
    void registerBindings(IUiBindingHost& host) override;

    // FPS overlay — actual display flushes/sec over a rolling 1s window.
    uint16_t fps()        const { return fps_; }
    bool     showFps()    const { return showFps_; }
    void     setShowFps(bool b) { showFps_ = b; }

    // Theme is Aether-owned (ADR 0002 — not on the IDisplayServer contract).
    // renderFrame() installs it as the active theme before drawing.
    void setTheme(const StyleTokens& t) { theme_ = &t; }
    const StyleTokens* theme() const { return theme_; }

private:
    IClock&  clock_;
    const StyleTokens* theme_ = nullptr;   // nullptr → defaultTheme() at render
    bool     showFps_     = false;
    uint32_t fpsFrames_   = 0;
    uint64_t fpsLastMs_   = 0;
    uint16_t fps_         = 0;
    uint16_t lastDrawMs_  = 0;   // time in active screen draw()
    uint16_t lastFlushMs_ = 0;   // time in canvas/LCD flush()
};

} // namespace nema
