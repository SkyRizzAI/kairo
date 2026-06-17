#include "nema/system/capabilities.h"
#include "nema/services/gui_service.h"
#include "nema/ui/style_tokens.h"
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
            if (t == "compact")     aether_->setServerTheme(compactTheme());
            else if (t == "large")  aether_->setServerTheme(largeTheme());
            else                    aether_->setServerTheme(defaultTheme());
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
    while (!thread_.shouldStop()) {
        uint64_t now = rt_.clock().millis();
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

        // 5. Render — skip while sleeping; flush one blank frame on sleep entry.
        if (hasDisplay) {
            if (dpm_.isSleeping() && dpm_.takeEnteredSleep()) {
                rt_.canvas().clear(false);
                rt_.canvas().flush();
            } else if (!dpm_.isSleeping() && vd.takeRedraw()) {
                nema::ui::setRenderTick((uint32_t)now);
                // Restore the active server's presentational state before
                // rendering — theme and scale are server-owned, not global.
                if (const StyleTokens* st = server_->serverTheme())
                    setTheme(*st);
                else
                    setTheme(defaultTheme());
                rt_.canvas().setScale(server_->serverScale());
                server_->renderFrame(rt_.canvas(), vd, status_);
            }
        }

        nema::Thread::sleepMs(5);   // tighter loop → quicker redraw pickup
    }
}

} // namespace nema
