#include "kairo/apps/camera_app.h"
#include "kairo/runtime.h"
#include "kairo/clock.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/services/camera_service.h"
#include "kairo/hal/camera.h"
#include "kairo/input/input_action.h"
#include <cstring>
#include <cstdio>

namespace kairo {

CameraApp::CameraApp(Runtime& rt) : rt_(rt) {}

void CameraApp::enter() {
    cam_ = nullptr;
    if (rt_.camera().count() > 0)
        cam_ = rt_.camera().get(0);
    if (cam_ && !cam_->isOpen())
        cam_->open();
    fps_         = 0;
    frameCount_  = 0;
    fpsWindowMs_ = rt_.clock().millis();
    rt_.view().requestRedraw();
}

void CameraApp::tick(uint64_t nowMs) {
    // Update FPS counter every second
    if (nowMs - fpsWindowMs_ >= 1000) {
        fps_         = frameCount_;
        frameCount_  = 0;
        fpsWindowMs_ = nowMs;
    }
    rt_.view().requestRedraw();
}

void CameraApp::update(Key key) {
    if (key == Key::Cancel) {
        if (cam_) cam_->close();
        rt_.view().pop();
    }
}

void CameraApp::buildDitherBuf(const uint8_t* rgb, uint16_t fw, uint16_t fh) {
    memset(ditherBuf_, 0, kDitherBytes);
    uint16_t cropX = (fw > kDitherW) ? (fw - kDitherW) / 2 : 0;
    uint16_t rows  = (fh < kDitherH) ? fh : kDitherH;
    for (uint16_t row = 0; row < rows; row++) {
        for (uint16_t col = 0; col < kDitherW && col < fw; col++) {
            size_t   idx = ((size_t)row * fw + cropX + col) * 2;
            uint16_t px  = ((uint16_t)rgb[idx] << 8) | rgb[idx + 1];
            uint8_t  r   = (px >> 11) & 0x1F;
            uint8_t  g   = (px >>  5) & 0x3F;
            uint8_t  b   =  px        & 0x1F;
            // Luminance approximation — no floats, threshold at ~50%
            uint32_t lum = (uint32_t)r * 9 + (uint32_t)g * 9 + (uint32_t)b * 4;
            if (lum > 140) {
                size_t bitIdx = (size_t)row * kDitherW + col;
                ditherBuf_[bitIdx / 8] |= (uint8_t)(0x80 >> (bitIdx % 8));
            }
        }
    }
}

void CameraApp::draw(Canvas& c) {
    c.clear();

    if (!cam_) {
        c.drawText(c.centerX("No camera"), ui::CONTENT_Y + 20, "No camera", true);
        char hint[32];
        std::snprintf(hint, sizeof(hint), "%s back",
            rt_.input().hintFor(input::Action::Back));
        c.drawText(4, ui::footerY(c.height()), hint, true);
        return;
    }

    const uint8_t* frame = cam_->captureFrame();
    if (frame) {
        frameCount_++;
        const uint16_t fw    = cam_->frameWidth();   // 320
        const uint16_t fh    = cam_->frameHeight();  // 240
        const uint16_t dstW  = (fw < (uint16_t)c.width()) ? fw : (uint16_t)c.width();
        const uint16_t cropX = (fw > dstW) ? (fw - dstW) / 2 : 0;
        const uint16_t rows  = (fh < (uint16_t)c.height()) ? fh : (uint16_t)c.height();

        // Blit RGB565 row by row — full color, bypasses 1-bit framebuffer
        for (uint16_t row = 0; row < rows; row++) {
            const uint8_t* rowPtr = frame + ((size_t)row * fw + cropX) * 2;
            c.blitRgb565(rowPtr, 0, row, dstW, 1);
        }
    }

    // FPS overlay (drawn on top via 1-bit path — white text on color bg)
    char fpsBuf[8];
    std::snprintf(fpsBuf, sizeof(fpsBuf), "%dfps", (int)fps_);
    c.drawText((uint16_t)(c.width() - c.textWidth(fpsBuf) - 2), 2, fpsBuf, true);

    // Back hint
    char hint[32];
    std::snprintf(hint, sizeof(hint), "%s back",
        rt_.input().hintFor(input::Action::Back));
    c.drawText(4, ui::footerY(c.height()), hint, true);
}

} // namespace kairo
