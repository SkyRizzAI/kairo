#pragma once
#include "kairo/ui/screen.h"
#include <cstdint>
#include <cstddef>

namespace kairo {

class Runtime;
struct ICamera;

// CameraApp — live 1-bit grayscale viewfinder.
// Captures RGB565 frames from the first registered ICamera, thresholds to
// 1-bit luminance, and renders via Canvas::drawBitmap. Fullscreen mode.
class CameraApp : public IScreen {
public:
    explicit CameraApp(Runtime& rt);

    ScreenMode mode()               const override { return ScreenMode::Fullscreen; }
    bool       suppressCanvasFlush() const override { return true; }

    void enter()              override;
    void tick(uint64_t nowMs) override;
    void update(Key key)      override;
    void draw(Canvas& c)      override;

private:
    void buildDitherBuf(const uint8_t* rgb565, uint16_t fw, uint16_t fh);

    Runtime&  rt_;
    ICamera*  cam_          = nullptr;
    uint8_t   fps_          = 0;
    uint8_t   frameCount_   = 0;
    uint64_t  fpsWindowMs_  = 0;

    static constexpr uint16_t kDitherW     = 240;
    static constexpr uint16_t kDitherH     = 240;
    static constexpr size_t   kDitherBytes = ((size_t)kDitherW * kDitherH + 7) / 8;
    uint8_t ditherBuf_[kDitherBytes] = {};
};

} // namespace kairo
