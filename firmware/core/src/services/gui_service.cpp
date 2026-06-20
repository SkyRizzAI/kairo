#include "nema/system/capabilities.h"
#include "nema/services/gui_service.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/animation_manager.h"
#include "nema/ui/font_registry.h"
#include "nema/runtime.h"
#include "nema/clock.h"
#include "nema/service/service_container.h"
#include "nema/config/config_store.h"
#include "nema/ui/canvas.h"
#include "nema/ui/renderer.h"
#include "nema/ui/screen.h"
#include "nema/ui/status_bar.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/services/input_service.h"
#include "nema/app/app_host_manager.h"
#include "nema/input/input_action.h"
#include "nema/system/capability_registry.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include "nema/task_runner.h"
#include <ctime>
#include <string>

namespace nema {

void GuiService::start() {
    // Plan 70: register built-in fonts with the FontRegistry.
    auto& fontReg = aether::ui::FontRegistry::instance();
    // Explicit size/weight handles (proportional Helvetica, Plan 79).
    fontReg.registerFont(aether::ui::Fonts::Reg8,   &FONT_REG8,   "reg8");
    fontReg.registerFont(aether::ui::Fonts::Bold8,  &FONT_BOLD8,  "bold8");
    fontReg.registerFont(aether::ui::Fonts::Reg10,  &FONT_REG10,  "reg10");
    fontReg.registerFont(aether::ui::Fonts::Bold10, &FONT_BOLD10, "bold10");
    fontReg.registerFont(aether::ui::Fonts::Reg12,  &FONT_REG12,  "reg12");
    fontReg.registerFont(aether::ui::Fonts::Bold12, &FONT_BOLD12, "bold12");
    // Role handles map onto the family: titles/subheaders bold-10, body regular-8,
    // tiny regular-8, big numbers bold-12, mono stays the 6×8 fixed-width font.
    fontReg.registerFont(aether::ui::Fonts::Primary,   &FONT_BOLD10, "primary");
    fontReg.registerFont(aether::ui::Fonts::Secondary, &FONT_REG8,   "secondary");
    fontReg.registerFont(aether::ui::Fonts::Mono,      &FONT_6X8,    "mono");
    fontReg.registerFont(aether::ui::Fonts::Tiny,      &FONT_REG8,   "tiny");
    fontReg.registerFont(aether::ui::Fonts::BigNum,    &FONT_BOLD12, "bignum");

    display_ = rt_.container().resolve<IDisplayDriver>();

    // Display servers (Plan 43). Default = AetherServer (1-bit canvas UI);
    // FbconServer is the swappable text-console backend. server_ is the active
    // one; switch at runtime via requestServer()/the CLI `display` command.
    aether_ = std::make_unique<AetherServer>(rt_.clock());
    fbcon_    = std::make_unique<FbconServer>(rt_);
    // CLI-first by default (Plan 43): boot lands in the fbcon console, like a
    // Linux TTY — the UI is launched on top via `display start aether`. A board
    // or user can opt into booting straight to the UI with config display/boot.
    server_   = fbcon_.get();

    // Load saved timeouts — fall back to 15s defaults if not set yet
    uint64_t sleepMs = 15000, lockMs = 15000;
    if (auto* cfg = rt_.container().resolve<IConfigStore>()) {
        sleepMs = (uint64_t)cfg->getIntOr("dpm", "sleep_ms", (int64_t)sleepMs);
        lockMs  = (uint64_t)cfg->getIntOr("dpm", "lock_ms",  (int64_t)lockMs);
        aether_->setShowFps(cfg->getIntOr("debug", "fps", 0) != 0);  // Settings → Display
        // Aether owns its presentational state (theme + scale). FbCon is a
        // text console — it always renders at scale 1 with the default theme.
        {
            std::string t = cfg->getString("display", "theme", "default");
            if (t == "compact")     aether_->setTheme(aether::compactTheme());
            else if (t == "large")  aether_->setTheme(aether::largeTheme());
            else                    aether_->setTheme(aether::defaultTheme());
        }
        // Snapshot the canvas scale that runtime.cpp resolved from config/DPI
        // so Aether can restore it when switching back from another server.
        if (rt_.canvas().scale() >= 1.0f)
            aether_->setServerScale(rt_.canvas().scale());
        // Boot server policy (Plan 43): board/user picks the initial backend via
        // config "display/boot". Default = fbcon (CLI-first console boot); set
        // "aether" to boot straight into the UI. No core default autostart.
        if (cfg->getString("display", "boot", "fbcon") == "aether")
            server_ = aether_.get();
    }

    // Crash/fault fallback (Plan 43 Fase 4): if the display resource faults or
    // detaches, drop to the fbcon console so the device never goes dark. Runs on
    // the main thread (EventBus dispatch); requestServer() is thread-safe.
    rt_.events().subscribe(events::ResourceChanged, [this](const Event& e) {
        bool isDisplay = false, notAvailable = false;
        for (const auto& f : e.payload) {
            if (std::string(f.key) == "resource") isDisplay    = (f.value == caps::Display);
            if (std::string(f.key) == "state")    notAvailable = (f.value != "available");
        }
        if (isDisplay && notAvailable) requestServer("fbcon");
    });
    rt_.events().subscribe(events::BatteryChanged, [this](const Event& e) {
        for (const auto& f : e.payload) {
            if (std::string(f.key) == "level") status_.battery = std::stoi(f.value);
        }
    });

    dpm_.init(rt_.view(), display_, rt_.clock(), lockScreen_, sleepMs, lockMs);
    lockScreen_.setDpm(dpm_);
    rt_.view().requestRedraw();   // paint the boot backend immediately (esp. fbcon)
    // UI thread on core 1 (Arduino loop also core 1 but now near-idle).
    // Priority above the near-idle main loop so input/render stay snappy.
    thread_.start({"nema_gui", 8192, 6, 1}, &GuiService::threadEntry, this);
}

void GuiService::stop() {
    thread_.requestStop();
    thread_.join();
}

void GuiService::registerServer(IDisplayServer* s) {
    if (s) extraServers_.push_back(s);
}

bool GuiService::requestServer(const char* name) {
    IDisplayServer* target = findServer(name);
    if (!target) return false;
    pendingServer_.store(target);   // GUI thread applies it next iteration
    return true;
}

const char* GuiService::activeServerName() const {
    return server_ ? server_->name() : "none";
}

std::vector<const char*> GuiService::serverNames() const {
    std::vector<const char*> v;
    if (aether_) v.push_back(aether_->name());
    if (fbcon_)    v.push_back(fbcon_->name());
    for (IDisplayServer* s : extraServers_) v.push_back(s->name());
    return v;
}

IDisplayServer* GuiService::findServer(const char* name) const {
    if (!name) return nullptr;
    if (aether_ && std::string(aether_->name()) == name) return aether_.get();
    if (fbcon_  && std::string(fbcon_->name())  == name) return fbcon_.get();
    for (IDisplayServer* s : extraServers_)
        if (std::string(s->name()) == name) return s;
    return nullptr;
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
    // Plan 70: target ~30 fps (33 ms). On headless servers we spin tighter
    // for lower input latency; the sleep is the budget that remains.
    constexpr uint32_t TARGET_FRAME_MS = 33;

    while (!thread_.shouldStop()) {
        uint64_t frameStart = rt_.clock().millis();
        uint64_t now = frameStart;
        auto& vd = rt_.view();

        // 0. Apply a pending display-server swap (requested from another thread,
        //    e.g. the CLI). Done here so server_ is only ever touched by this
        //    thread. Force a redraw so the new backend paints immediately.
        if (auto* next = pendingServer_.exchange(nullptr)) {
            server_ = next;
            vd.requestRedraw();
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
                // Plan 22: long-hold → pause the foreground app (back to home).
                // Intercepted here so the app never sees it.
                if (ie.action == input::Action::Pause) {
                    rt_.appHost().pauseForeground();
                    continue;
                }
                if (!dpm_.deliverKey(ie.key, now)) {
                    if (!server_->onAction(ie.action)) {
                        vd.handleAction(ie.action);  // primary: Action-based dispatch
                        vd.handleCode(ie.code);       // secondary: raw code
                    }
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

        // 5. Plan 70: Tick animations. If any frame advanced, we need a redraw.
        if (anim::AnimationManager::instance().tickAll((uint32_t)now))
            vd.requestRedraw();

        // 6. Render — skip while sleeping; flush one blank frame on sleep entry.
        if (hasDisplay) {
            if (dpm_.isSleeping() && dpm_.takeEnteredSleep()) {
                rt_.canvas().clear(false);
                rt_.canvas().flush();
            } else if (!dpm_.isSleeping() && vd.takeRedraw()) {
                aether::ui::setRenderTick((uint32_t)now);
                // Theme is applied by the server itself in renderFrame (ADR 0002).
                // GuiService only drives the shared canvas scale (neutral).
                rt_.canvas().setScale(server_->serverScale());
                // Plan 70: partial redraw — clip to dirty region if available.
                uint16_t dx, dy, dw, dh;
                if (vd.getDirtyBounds(dx, dy, dw, dh))
                    rt_.canvas().setClip(dx, dy, dw, dh);
                server_->renderFrame(rt_.canvas(), vd, status_);
                rt_.canvas().clearClip();
            }
        }

        // Plan 70 frame pacing: sleep only the remaining budget so frames
        // arrive at a steady cadence. If we overshot the target the loop
        // spins again immediately (sleepMs(0) is a yield).
        uint64_t elapsed = rt_.clock().millis() - frameStart;
        if (elapsed < TARGET_FRAME_MS)
            nema::Thread::sleepMs((uint32_t)(TARGET_FRAME_MS - elapsed));
    }
}

} // namespace nema
