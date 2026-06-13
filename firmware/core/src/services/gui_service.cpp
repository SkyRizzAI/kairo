#include "nema/system/capabilities.h"
#include "nema/services/gui_service.h"
#include "nema/runtime.h"
#include "nema/clock.h"
#include "nema/service/service_container.h"
#include "nema/config/config_store.h"
#include "nema/ui/canvas.h"
#include "nema/ui/screen.h"
#include "nema/ui/status_bar.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/services/input_service.h"
#include "nema/app/app_host_manager.h"
#include "nema/input/input_action.h"
#include "nema/system/capability_registry.h"
#include "nema/task_runner.h"
#include <ctime>
#include <cstdio>

namespace nema {

void GuiService::start() {
    display_ = rt_.container().resolve<IDisplayDriver>();

    // Default display server = PixelateServer (1-bit canvas UI). server_ is the
    // swap point for future backends (fbcon, LVGL) — Plan 43.
    pixelate_ = std::make_unique<PixelateServer>(rt_.clock());
    server_   = pixelate_.get();

    // Load saved timeouts — fall back to 15s defaults if not set yet
    uint64_t sleepMs = 15000, lockMs = 15000;
    if (auto* cfg = rt_.container().resolve<IConfigStore>()) {
        sleepMs = (uint64_t)cfg->getIntOr("dpm", "sleep_ms", (int64_t)sleepMs);
        lockMs  = (uint64_t)cfg->getIntOr("dpm", "lock_ms",  (int64_t)lockMs);
        pixelate_->setShowFps(cfg->getIntOr("debug", "fps", 0) != 0);  // Settings → Display
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
    status_.wifi = rt_.capabilities().has(caps::NetWifi);
    rt_.view().requestRedraw();
}

void GuiService::loop() {
    const bool hasDisplay = rt_.capabilities().has(caps::Display);
    while (!thread_.shouldStop()) {
        uint64_t now = rt_.clock().millis();
        auto& vd = rt_.view();

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
                // Plan 22: long-hold → pause the foreground app (back to home).
                // Intercepted here so the app never sees it.
                if (ie.action == input::Action::Pause) {
                    rt_.appHost().pauseForeground();
                    continue;
                }
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
        //    can safely touch screen/app UI state.
        rt_.tasks().drainCompletions();

        // 5. Render — skip while sleeping; flush one blank frame on sleep entry.
        if (hasDisplay) {
            if (dpm_.isSleeping() && dpm_.takeEnteredSleep()) {
                rt_.canvas().clear(false);
                rt_.canvas().flush();
            } else if (!dpm_.isSleeping() && vd.takeRedraw()) {
                server_->renderFrame(rt_.canvas(), vd, status_);
            }
        }

        nema::Thread::sleepMs(5);   // tighter loop → quicker redraw pickup
    }
}

} // namespace nema
