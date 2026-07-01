#include "nema/system/capabilities.h"
#include "nema/services/gui_service.h"
#include "nema/ui/animation_manager.h"
#include "nema/ui/font_registry.h"
#include "nema/ui/display_server.h"
#include "nema/runtime.h"
#include "nema/clock.h"
#include "nema/service/service_container.h"
#include "nema/config/config_store.h"
#include "nema/ui/canvas.h"
#include "nema/ui/renderer.h"
#include "nema/ui/screen.h"
#include "nema/ui/status_bar.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/services/input_service.h"
#include "nema/app/app_host_manager.h"
#include "nema/services/permission_service.h"
#include "nema/input/input_action.h"
#include "nema/system/capability_registry.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include "nema/task_runner.h"
#include <ctime>
#include <string>

namespace nema {

void GuiService::start() {
    // Plan 70: register built-in fonts with the FontRegistry (nema::display).
    auto& fontReg = nema::display::FontRegistry::instance();
    // Default: proportional Helvetica family (Plan 79, generated from u8g2 BDFs).
    // User can override via Settings > Appearances > Font (saved to config "aether"/"font").
    fontReg.registerFont(nema::display::Fonts::Reg8,   &nema::display::FONT_REG8,   "reg8");
    fontReg.registerFont(nema::display::Fonts::Bold8,  &nema::display::FONT_BOLD8,  "bold8");
    fontReg.registerFont(nema::display::Fonts::Reg10,  &nema::display::FONT_REG10,  "reg10");
    fontReg.registerFont(nema::display::Fonts::Bold10, &nema::display::FONT_BOLD10, "bold10");
    fontReg.registerFont(nema::display::Fonts::Reg12,  &nema::display::FONT_REG12,  "reg12");
    fontReg.registerFont(nema::display::Fonts::Bold12, &nema::display::FONT_BOLD12, "bold12");
    fontReg.registerFont(nema::display::Fonts::Primary,   &nema::display::FONT_BOLD10, "primary");
    fontReg.registerFont(nema::display::Fonts::Secondary, &nema::display::FONT_REG8,   "secondary");
    fontReg.registerFont(nema::display::Fonts::Mono,      &nema::display::FONT_6X8,    "mono");
    fontReg.registerFont(nema::display::Fonts::Tiny,      &nema::display::FONT_REG8,   "tiny");
    fontReg.registerFont(nema::display::Fonts::BigNum,    &nema::display::FONT_BOLD12, "bignum");

    // Apply user-selected font pack if configured (overrides compiled-in defaults).
    {
        std::string fontPack = rt_.config().getString("aether", "font", "");
        if (!fontPack.empty() && fontPack != "Helvetica") {
            std::string packPath = "/system/assets/fonts/" + fontPack;
            if (!fontReg.applyFontPack(rt_.fs(), packPath.c_str())) {
                // Try SD card
                fontReg.applyFontPack(rt_.fs(), ("/ext/assets/fonts/" + fontPack).c_str());
            }
        }
    }

    display_ = rt_.container().resolve<IDisplayDriver>();

    // Load saved DPM timeouts — fall back to 15s defaults if not set yet.
    // (The display servers + their presentational state (theme/scale/fps) are
    // configured by the target main, which owns them — Plan 80.)
    uint64_t sleepMs = 15000, lockMs = 15000;
    if (auto* cfg = rt_.container().resolve<IConfigStore>()) {
        sleepMs = (uint64_t)cfg->getIntOr("dpm", "sleep_ms", (int64_t)sleepMs);
        lockMs  = (uint64_t)cfg->getIntOr("dpm", "lock_ms",  (int64_t)lockMs);
    }

    // Crash/fault fallback (Plan 43 Fase 4): if the display resource faults or
    // detaches, drop to the fbcon console so the device never goes dark. Runs on
    // the main thread (EventBus dispatch); switchDisplayServer() is thread-safe.
    rt_.events().subscribe(events::ResourceChanged, [this](const Event& e) {
        bool isDisplay = false, notAvailable = false;
        for (const auto& f : e.payload) {
            if (std::string(f.key) == "resource") isDisplay    = (f.value == caps::Display);
            if (std::string(f.key) == "state")    notAvailable = (f.value != "available");
        }
        if (isDisplay && notAvailable) rt_.switchDisplayServer("fbcon");
    });
    rt_.events().subscribe(events::BatteryChanged, [this](const Event& e) {
        for (const auto& f : e.payload) {
            if (std::string(f.key) == "level") status_.battery = std::stoi(f.value);
        }
    });

    rt_.dpm().init(rt_.view(), display_, rt_.clock(), lockScreen_, sleepMs, lockMs);
    lockScreen_.setDpm(rt_.dpm());

    // Plan 87 Fase 1: register PermissionScreen factory so the service can push
    // the modal from the GUI thread without a core→aether link dependency.
    if (auto* perm = rt_.container().resolve<PermissionService>()) {
        perm->setScreenFactory([this](auto req, auto& vd) {
            permScreen_.prepare(req);
            vd.navigate(permScreen_);
        });
    }

    // Plan 94: wallet sign-consent modal (trusted display) — same pattern.
    if (auto* cs = rt_.container().resolve<wallet::WalletConsentService>()) {
        cs->setScreenFactory([this](auto req) {
            signConsentScreen_.prepare(req);
            rt_.view().navigate(signConsentScreen_);
        });
    }
    rt_.view().requestRedraw();   // paint the boot backend immediately (esp. fbcon)
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
    // Status Bar ON/OFF (Plan 81) — read every frame so the toggle is immediate;
    // the rest of the status refresh is throttled to 10s below.
    status_.visible = rt_.config().getIntOr("aether", "statusbar", 1) != 0;
    nema::display::statusBarVisible = status_.visible;  // sync layout global
    if (lastStatusMs_ != 0 && now - lastStatusMs_ < 10000) return;
    lastStatusMs_ = now;
    time_t t = (time_t)(rt_.clock().epochMs() / 1000);
    if (struct tm* tm = localtime(&t)) {
        status_.hour   = tm->tm_hour;
        status_.minute = tm->tm_min;
    }
    status_.wifi = rt_.capabilities().available(caps::NetWifi);
    rt_.view().requestRedraw();
}

void GuiService::loop() {
    const bool hasDisplay = rt_.capabilities().has(caps::Display);
    // Plan 70: target ~30 fps (33 ms). On headless servers we spin tighter
    // for lower input latency; the sleep is the budget that remains.
    constexpr uint32_t TARGET_FRAME_MS = 33;
    auto& dpm = rt_.dpm();

    while (!thread_.shouldStop()) {
        uint64_t frameStart = rt_.clock().millis();
        uint64_t now = frameStart;
        auto& vd = rt_.view();

        // 0. Apply a pending display-server swap (requested from another thread,
        //    e.g. the CLI). Runtime holds the registry; we apply on this thread so
        //    the active server is only ever changed here. Force a redraw after.
        if (rt_.applyPendingServer()) vd.requestRedraw();
        IDisplayServer* server = rt_.displayServer();

        // 1. Input — DPM intercepts; only forwarded to the screen if not consumed.
        InputEvent ie;
        while (rt_.input().next(ie)) {
            if (ie.kind == InputEvent::Kind::Pointer) {
                // Touch: forward to active screen (apps relay to their mailbox).
                // v1: touch bypasses DPM sleep/lock (TODO: count touch as activity).
                // Touch drivers emit PHYSICAL panel pixels, but the whole UI (layout,
                // node hit-testing, drawRaw) works in LOGICAL canvas pixels
                // (physical / scale). Convert here so every consumer gets logical
                // coords — otherwise at scale=2 the pointer is 2× off (missed taps,
                // marker drift). No-op at scale=1.
                float sc = rt_.canvas().scale();
                uint16_t px = sc > 1.0f ? (uint16_t)(ie.px / sc) : ie.px;
                uint16_t py = sc > 1.0f ? (uint16_t)(ie.py / sc) : ie.py;
                vd.handlePointer(input::PointerEvent{ie.pphase, px, py});
                continue;
            }
            if (ie.type == InputEvent::Type::Press || ie.type == InputEvent::Type::Repeat) {
                // Plan 22: long-hold → pause the foreground app (back to home).
                // Intercepted here so the app never sees it.
                if (ie.action == input::Action::Pause) {
                    rt_.appHost().pauseForeground();
                    continue;
                }
                if (!dpm.deliverKey(ie.key, now)) {
                    if (!server || !server->onAction(ie.action)) {
                        auto* before = vd.active();
                        vd.handleAction(ie.action);  // primary: Action-based dispatch
                        // If the action navigated away (e.g. a modal's Approve/Allow
                        // closed it), do NOT also deliver the raw code to the now-active
                        // screen — that single press would otherwise bleed through and
                        // re-fire the control beneath the modal (sign-consent loop).
                        if (vd.active() == before)
                            vd.handleCode(ie.code);   // secondary: raw code
                    }
                }
            }
        }

        // 2. DPM state machine — may trigger sleep/lock transitions.
        dpm.tick(now);

        // 3. Status bar + screen tick.
        refreshStatus(now);
        vd.tick(now);

        // 3.5. Permission screen injection (Plan 87 Fase 1): push PermissionScreen
        //      when an app is blocking on perm.request() for a sensitive cap.
        if (auto* perm = rt_.container().resolve<PermissionService>())
            perm->guiTick(vd);

        // Wallet sign-consent injection (Plan 94): push SignConsentScreen when a
        // sign request is blocking on the trusted display.
        if (auto* cs = rt_.container().resolve<wallet::WalletConsentService>())
            cs->guiTick();

        // 4. Background job completions — run on THIS (UI) thread, so callbacks
        //    can safely touch screen/app UI state.
        rt_.tasks().drainCompletions();

        // 5. Plan 70: Tick animations. If any frame advanced, we need a redraw.
        // Skip while display is off (Sleep or Locked-before-wake) — avoids updating
        // LCD GRAM with animation frames that would flash briefly on backlight-on.
        if (!dpm.isDisplayOff() && anim::AnimationManager::instance().tickAll((uint32_t)now))
            vd.requestRedraw();

        // 6. Render — skip while display is off; flush one blank frame on sleep entry.
        if (hasDisplay && server) {
            if (dpm.isSleeping() && dpm.takeEnteredSleep()) {
                rt_.canvas().clear(false);
                rt_.canvas().flush();
            } else if (!dpm.isDisplayOff() && vd.takeRedraw()) {
                aether::ui::setRenderTick((uint32_t)now);
                // Theme is applied by the server itself in renderFrame (ADR 0002).
                // GuiService only drives the shared canvas scale (neutral).
                rt_.canvas().setScale(server->serverScale());
                // Plan 70: partial redraw — clip to dirty region if available.
                uint16_t dx, dy, dw, dh;
                if (vd.getDirtyBounds(dx, dy, dw, dh))
                    rt_.canvas().setClip(dx, dy, dw, dh);
                server->renderFrame(rt_.canvas(), vd, status_);
                rt_.canvas().clearClip();
            }
        }

        // Plan 97 — event-driven pacing. Park for the remaining frame budget, but
        // wake EARLY when a cross-thread producer signals: input posted
        // (InputService::post) or an app finished a frame (AppHost::present). This
        // removes the up-to-33 ms poll latency on both edges of the input→pixel
        // path while preserving the 30 fps ceiling (animation/DPM/status keep
        // ticking at most once per TARGET_FRAME_MS). GUI-thread-internal redraws
        // (animation/status/navigation) deliberately do NOT signal — they already
        // render in the current frame, and waking per-frame would busy-spin.
        uint64_t elapsed = rt_.clock().millis() - frameStart;
        if (elapsed < TARGET_FRAME_MS)
            rt_.guiWaker().wait((uint32_t)(TARGET_FRAME_MS - elapsed));
        else
            nema::Thread::sleepMs(0);   // overran budget: yield and spin
    }
}

} // namespace nema
