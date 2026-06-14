#include "nema/ui/aether_server.h"
#include "nema/ui/canvas.h"
#include "nema/ui/screen.h"
#include "nema/ui/status_bar.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/clock.h"
#include <cstdio>

namespace nema {

void AetherServer::renderFrame(Canvas& c, ViewDispatcher& vd, const StatusBarData& status) {
    uint64_t now = clock_.millis();
    // FPS window: snapshot the flush count once per second.
    if (now - fpsLastMs_ >= 1000) {
        fps_       = (uint16_t)fpsFrames_;
        fpsFrames_ = 0;
        fpsLastMs_ = now;
    }
    fpsFrames_++;   // count actual display flushes

    uint64_t tDraw0 = now;
    c.clear();
    if (auto* s = vd.active()) {
        switch (s->mode()) {
        case ScreenMode::Normal:
            StatusBar::draw(c, status);
            break;
        case ScreenMode::Modal: {
            if (auto* bg = vd.previous()) {
                if (bg->mode() == ScreenMode::Normal) StatusBar::draw(c, status);
                bg->draw(c);
            }
            uint16_t mw = s->modalWidth();
            uint16_t mh = s->modalHeight();
            uint16_t mx = (c.width()  - mw) / 2;
            uint16_t my = (c.height() - mh) / 2;
            c.fillRect(mx, my, mw, mh, false);
            c.drawRect(mx, my, mw, mh, true);
            break;
        }
        case ScreenMode::Fullscreen:
            break;
        }
        s->draw(c);

        // Fullscreen screens that use direct color rendering (blitRgb565) need
        // to suppress the 1-bit canvas flush — it would overwrite their content.
        if (s->mode() == ScreenMode::Fullscreen && s->suppressCanvasFlush())
            return;
    }
    lastDrawMs_ = (uint16_t)(clock_.millis() - tDraw0);

    // FPS + timing overlay (top-right): "<fps> d<drawMs>/f<flushMs>" so you can
    // see exactly where a slow frame goes (screen draw vs LCD flush).
    if (showFps_) {
        char fb[24];
        std::snprintf(fb, sizeof(fb), "%u d%u/f%u",
                      (unsigned)fps_, (unsigned)lastDrawMs_, (unsigned)lastFlushMs_);
        uint16_t tw = c.textWidth(fb);
        uint16_t bx = (uint16_t)(c.width() > tw + 4 ? c.width() - tw - 4 : 0);
        c.fillRect(bx, 0, (uint16_t)(tw + 4), ui::CHAR_H + 1, false);  // clear bg
        c.drawText((uint16_t)(bx + 2), 1, fb, true);
    }

    uint64_t tFlush0 = clock_.millis();
    c.flush();
    lastFlushMs_ = (uint16_t)(clock_.millis() - tFlush0);
}

} // namespace nema
