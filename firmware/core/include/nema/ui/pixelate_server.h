#pragma once
#include "nema/ui/display_server.h"
#include <cstdint>

namespace nema {

struct IClock;

// PixelateServer — the built-in 1-bit canvas renderer (the default display
// server). Composites the status bar + active screen/modal onto the Canvas and
// flushes, with an optional FPS/timing overlay. This is exactly the rendering
// that used to live inline in GuiService::renderOnce.
class PixelateServer : public IDisplayServer {
public:
    explicit PixelateServer(IClock& clock) : clock_(clock) {}

    const char* name() const override { return "pixelate"; }
    void renderFrame(Canvas& c, ViewDispatcher& views, const StatusBarData& status) override;

    // FPS overlay — actual display flushes/sec over a rolling 1s window.
    uint16_t fps()        const { return fps_; }
    bool     showFps()    const { return showFps_; }
    void     setShowFps(bool b) { showFps_ = b; }

private:
    IClock&  clock_;
    bool     showFps_     = false;
    uint32_t fpsFrames_   = 0;
    uint64_t fpsLastMs_   = 0;
    uint16_t fps_         = 0;
    uint16_t lastDrawMs_  = 0;   // time in active screen draw()
    uint16_t lastFlushMs_ = 0;   // time in canvas/LCD flush()
};

} // namespace nema
