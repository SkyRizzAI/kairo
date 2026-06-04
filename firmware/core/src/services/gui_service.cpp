#include "kairo/services/gui_service.h"
#include "kairo/runtime.h"
#include "kairo/clock.h"
#include "kairo/service/service_container.h"
#include "kairo/config/config_store.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/screen.h"
#include "kairo/ui/status_bar.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/services/input_service.h"
#include "kairo/system/capability_registry.h"
#include "kairo/nema/task_runner.h"
#include <ctime>

namespace kairo {

void GuiService::start() {
    display_ = rt_.container().resolve<IDisplayDriver>();

    // Load saved timeouts — fall back to 15s defaults if not set yet
    uint64_t sleepMs = 15000, lockMs = 15000;
    if (auto* cfg = rt_.container().resolve<IConfigStore>()) {
        sleepMs = (uint64_t)cfg->getIntOr("dpm", "sleep_ms", (int64_t)sleepMs);
        lockMs  = (uint64_t)cfg->getIntOr("dpm", "lock_ms",  (int64_t)lockMs);
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
    }
    c.flush();
}

void GuiService::loop() {
    const bool hasDisplay = rt_.capabilities().has("display");
    while (!thread_.shouldStop()) {
        uint64_t now = rt_.clock().millis();
        auto& vd = rt_.view();

        // 1. Input — DPM intercepts; only forwarded to the screen if not consumed.
        InputEvent ie;
        while (rt_.input().next(ie)) {
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

        nema::Thread::sleepMs(15);
    }
}

} // namespace kairo
