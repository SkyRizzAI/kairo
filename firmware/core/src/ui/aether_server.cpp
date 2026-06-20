// Plan 60 Fase 4 — AetherServer chrome update (modal → rounded box, FPS overlay).
#include "nema/ui/aether_server.h"
#include "nema/ui/aether_abi.h"
#include "nema/ui/draw.h"
#include "nema/system/capabilities.h"
#include "nema/ui/canvas.h"
#include "nema/ui/screen.h"
#include "nema/ui/status_bar.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/style_tokens.h"
#include "nema/clock.h"
#include <cstdio>

namespace nema {

void AetherServer::renderFrame(Canvas& c, ViewDispatcher& vd, const StatusBarData& status) {
    // Aether owns its theme (ADR 0002): install it as the active theme each frame
    // so it never bleeds in from another server. Default if none set.
    // (aether::setTheme = the global active-theme setter, not our member setTheme.)
    aether::setTheme(theme_ ? *theme_ : aether::defaultTheme());
    uint64_t now = clock_.millis();
    if (now - fpsLastMs_ >= 1000) {
        fps_       = (uint16_t)fpsFrames_;
        fpsFrames_ = 0;
        fpsLastMs_ = now;
    }
    fpsFrames_++;

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
            // Plan 70: dim the background with a 50% dither pattern before drawing the modal
            uint16_t mw = s->modalWidth();
            uint16_t mh = s->modalHeight();
            uint16_t mx = (uint16_t)((c.width()  > mw) ? (c.width()  - mw) / 2 : 0);
            uint16_t my = (uint16_t)((c.height() > mh) ? (c.height() - mh) / 2 : 0);
            // Dither backdrop: checkerboard pattern for 1-bit ~50% dim effect
            for (uint16_t row = 0; row < c.height(); row++)
                for (uint16_t col = (row % 2); col < c.width(); col += 2)
                    c.drawPixel(col, row, true);
            // Clear modal box interior + rounded border
            c.fillRect(mx, my, mw, mh, false);
            aether::ui::draw::box_rounded(c, mx, my, mw, mh, false);
            break;
        }
        case ScreenMode::Fullscreen:
            break;
        }
        s->draw(c);

        if (s->mode() == ScreenMode::Fullscreen && s->suppressCanvasFlush())
            return;
    }
    lastDrawMs_ = (uint16_t)(clock_.millis() - tDraw0);

    if (showFps_) {
        char fb[24];
        std::snprintf(fb, sizeof(fb), "%u d%u/f%u",
                      (unsigned)fps_, (unsigned)lastDrawMs_, (unsigned)lastFlushMs_);
        uint16_t tw = c.textWidth(fb);
        uint16_t bx = (uint16_t)(c.width() > tw + 4 ? c.width() - tw - 4 : 0);
        // Small rounded pill background for FPS counter
        aether::ui::draw::box_rounded(c, bx, 0, (uint16_t)(tw + 4),
                                      aether::ui::CHAR_H + 1, true);
        c.drawText((uint16_t)(bx + 2), 1, fb, false);  // white text on dark pill
    }

    uint64_t tFlush0 = clock_.millis();
    c.flush();
    lastFlushMs_ = (uint16_t)(clock_.millis() - tFlush0);
}

const UiSdkDescriptor* AetherServer::uiSdk() const {
    static const char* caps[] = { caps::Display, caps::Input2D };
    static const UiSdkDescriptor d{ "aether:ui", 1, 0, caps, 2 };
    return &d;
}

void AetherServer::registerBindings(IUiBindingHost& host) {
    host.bind("aether:ui", "view-begin",    (void*)&aether_view_begin);
    host.bind("aether:ui", "view-end",      (void*)&aether_view_end);
    host.bind("aether:ui", "label",         (void*)&aether_text_label);
    host.bind("aether:ui", "styled",        (void*)&aether_text_styled);
    host.bind("aether:ui", "button",        (void*)&aether_interactive_button);
    host.bind("aether:ui", "scroll-begin",  (void*)&aether_scroll_begin);
    host.bind("aether:ui", "scroll-end",    (void*)&aether_scroll_end);
}

} // namespace nema
