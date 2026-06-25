// SplashScreen (Plan 92) — cat logo + a 2-second progress bar. Pure presentation
// (lives in the aether layer; the kernel is never involved in the splash).
#include "nema/screens/splash_screen.h"
#include "nema/runtime.h"
#include "nema/ui/canvas.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/assets/splash_logo.h"
#include "aether/shell/blit.h"   // blitScaledMask — same scaler the desktop uses

namespace nema {

static constexpr uint16_t kLogoW   = 128, kLogoH = 64;
static constexpr uint64_t kSplashMs = 2000;

SplashScreen::SplashScreen(Runtime& rt, Mode mode)
    : ComponentScreen(rt), mode_(mode) {}

void SplashScreen::onResume() {
    startMs_ = 0; progress_ = 0.0f; done_ = false;
    requestRedraw();
}

void SplashScreen::draw(Canvas& c) {
    int W = c.width(), H = c.height();
    c.clear(false);   // black background

    // Logo scaled to fit the LOGICAL resolution (≤ 4/5 width, ≤ 1/2 height), keeping
    // the 2:1 aspect. Uses the same scaler the desktop wallpaper uses (blitScaledMask).
    int tw = W * 4 / 5;
    int th = tw * (int)kLogoH / (int)kLogoW;
    if (th > H / 2) { th = H / 2; tw = th * (int)kLogoW / (int)kLogoH; }
    if (tw < 8) tw = 8;
    if (th < 4) th = 4;

    // The logo + progress bar are centred as a group (fit-centre, both axes).
    const int bh = 5, vgap = 10;
    int bw = W / 2;
    int blockH = th + vgap + bh;
    int y0 = (H - blockH) / 2;            // logo top
    if (y0 < 0) y0 = 0;
    int x0 = (W - tw) / 2;
    nema::shell::blitScaledMask(c, kSplashLogo, (int)kLogoW, (int)kLogoH,
                                x0, y0, tw, th, /*color=*/true);

    // Progress bar below the logo — rounded outline + fill proportional to elapsed time.
    int bx = (W - bw) / 2, by = y0 + th + vgap;
    c.drawRoundRect((uint16_t)bx, (uint16_t)by, (uint16_t)bw, (uint16_t)bh, 2, true);
    int fw = (int)(progress_ * (float)(bw - 4));
    if (fw > 0)
        c.fillRoundRect((uint16_t)(bx + 2), (uint16_t)(by + 1), (uint16_t)fw, (uint16_t)(bh - 2), 1, true);
}

void SplashScreen::tick(uint64_t nowMs) {
    if (done_) return;
    if (startMs_ == 0) startMs_ = nowMs;
    uint64_t el = nowMs - startMs_;
    progress_ = el >= kSplashMs ? 1.0f : (float)el / (float)kSplashMs;
    rt_.view().requestRedraw();
    if (el >= kSplashMs) {
        done_ = true;
        if      (mode_ == Mode::Restart)  rt_.requestRestart();
        else if (mode_ == Mode::Shutdown) rt_.requestShutdown();
        else                              rt_.view().goBack();   // Boot → reveal the desktop
    }
}

} // namespace nema
