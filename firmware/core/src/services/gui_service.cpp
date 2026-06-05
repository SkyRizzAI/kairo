#include "kairo/services/gui_service.h"
#include "kairo/runtime.h"
#include "kairo/clock.h"
#include "kairo/service/service_container.h"
#include "kairo/config/config_store.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/screen.h"
#include "kairo/ui/status_bar.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/services/input_service.h"
#include "kairo/system/capability_registry.h"
#include "kairo/nema/task_runner.h"
#include <ctime>
#include <cstdio>

namespace kairo {

void GuiService::start() {
    display_ = rt_.container().resolve<IDisplayDriver>();

    // Load saved timeouts — fall back to 15s defaults if not set yet
    uint64_t sleepMs = 15000, lockMs = 15000;
    if (auto* cfg = rt_.container().resolve<IConfigStore>()) {
        sleepMs = (uint64_t)cfg->getIntOr("dpm", "sleep_ms", (int64_t)sleepMs);
        lockMs  = (uint64_t)cfg->getIntOr("dpm", "lock_ms",  (int64_t)lockMs);
        showFps_ = cfg->getIntOr("debug", "fps", 0) != 0;  // toggle in Settings → Display
    }

    dpm_.init(rt_.view(), display_, rt_.clock(), lockScreen_, sleepMs, lockMs);
    lockScreen_.setDpm(dpm_);
    // UI thread on core 1 (Arduino loop also core 1 but now near-idle).
    // Priority above the near-idle main loop so input/render stay snappy.
    thread_.start({"nema_gui", 8192, 6, 1}, &GuiService::threadEntry, this);
}

void GuiService::stop() {
    thread_.requestStop();
    thread_.join();
}

void GuiService::threadEntry(void* self) {
    static_cast<GuiService*>(self)->loop();
}

void GuiService::refreshStatus(uint64_t now) {
    if (lastStatusMs_ != 0 && now - lastStatusMs_ < 10000) return;
    lastStatusMs_ = now;
    time_t t = (time_t)(rt_.clock().epochMs() / 1000);
    if (struct tm* tm = localtime(&t)) {
        status_.hour   = tm->tm_hour;
        status_.minute = tm->tm_min;
    }
    status_.wifi = rt_.capabilities().has("wifi");
    rt_.view().requestRedraw();
}

void GuiService::renderOnce(Canvas& c) {
    auto& vd = rt_.view();
    fpsFrames_++;   // count actual display flushes
    uint64_t tDraw0 = rt_.clock().millis();
    c.clear();
    if (auto* s = vd.active()) {
        switch (s->mode()) {
        case ScreenMode::Normal:
            StatusBar::draw(c, status_);
            break;
        case ScreenMode::Modal: {
            if (auto* bg = vd.previous()) {
                if (bg->mode() == ScreenMode::Normal) StatusBar::draw(c, status_);
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
    lastDrawMs_ = (uint16_t)(rt_.clock().millis() - tDraw0);

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

    uint64_t tFlush0 = rt_.clock().millis();
    c.flush();
    lastFlushMs_ = (uint16_t)(rt_.clock().millis() - tFlush0);
}

void GuiService::loop() {
    const bool hasDisplay = rt_.capabilities().has("display");
    while (!thread_.shouldStop()) {
        uint64_t now = rt_.clock().millis();
        auto& vd = rt_.view();

        // FPS window: snapshot flush count once per second.
        if (now - fpsLastMs_ >= 1000) {
            fps_ = (uint16_t)fpsFrames_;
            fpsFrames_ = 0;
            fpsLastMs_ = now;
        }

        // 1. Input — DPM intercepts; only forwarded to the screen if not consumed.
        InputEvent ie;
        while (rt_.input().next(ie)) {
            if (ie.kind == InputEvent::Kind::Pointer) {
                // Touch: forward to active screen (apps relay to their mailbox).
                // v1: touch bypasses DPM sleep/lock (TODO: count touch as activity).
                vd.handlePointer(input::PointerEvent{ie.pphase, ie.px, ie.py});
                continue;
            }
            if (ie.type == InputEvent::Type::Press || ie.type == InputEvent::Type::Repeat) {
                if (!dpm_.deliverKey(ie.key, now)) {
                    vd.handleAction(ie.action);  // primary: Action-based dispatch
                    vd.handleCode(ie.code);       // secondary: raw code
                }
            }
        }

        // 2. DPM state machine — may trigger sleep/lock transitions.
        dpm_.tick(now);

        // 3. Status bar + screen tick.
        refreshStatus(now);
        vd.tick(now);

        // 4. Background job completions — run on THIS (UI) thread, so callbacks
        //    can safely touch screen/plugin UI state.
        rt_.tasks().drainCompletions();

        // 5. Render — skip while sleeping; flush one blank frame on sleep entry.
        if (hasDisplay) {
            if (dpm_.isSleeping() && dpm_.takeEnteredSleep()) {
                rt_.canvas().clear(false);
                rt_.canvas().flush();
            } else if (!dpm_.isSleeping() && vd.takeRedraw()) {
                renderOnce(rt_.canvas());
            }
        }

        nema::Thread::sleepMs(5);   // tighter loop → quicker redraw pickup
    }
}

} // namespace kairo
