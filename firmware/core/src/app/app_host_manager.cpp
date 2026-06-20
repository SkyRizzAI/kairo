#include "nema/app/app_host_manager.h"
#include "nema/app/app_host.h"
#include "nema/app/app.h"
#include "nema/app/app_registry.h"
#include "nema/ui/screen.h"   // IScreen — modal_ is a server-supplied IScreen (Plan 80)
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/event/event.h"
#include "nema/event/event_bus.h"
#include "nema/ui/view_dispatcher.h"

namespace nema {

AppHostManager::AppHostManager(Runtime& rt) : rt_(rt) {}
AppHostManager::~AppHostManager() = default;

void AppHostManager::initCrashRecovery() {
    rt_.events().subscribe(events::AppHostExited,
        [this](const Event& e) { onAppExited(e); });
    rt_.events().subscribe(events::ClockTick,
        [this](const Event& e) { onClockTick(e); });
}

void AppHostManager::doLaunch(IApp& app) {
    foreground_.reset();   // free the previous (now-finished) host
    foreground_ = std::make_unique<AppHost>(rt_, app);
    rt_.log().info("AppHostManager", "launch", {{"app", app.name()}});
    rt_.view().push(*foreground_);
}

void AppHostManager::launch(IApp& app) {
    if (paused_) {
        if (modalFactory_) {
            // Display server installed a transition modal ("Close & Open?").
            modal_ = modalFactory_(rt_, *this, app);
            if (modal_) { rt_.view().push(*modal_); return; }
        }
        // Headless / no transition UI: enforce the single-slot policy directly.
        killPaused();
    }
    doLaunch(app);
}

void AppHostManager::pauseForeground() {
    if (!foreground_ || rt_.view().active() != foreground_.get()) return;
    foreground_->setPaused(true);
    rt_.log().info("AppHostManager", "pause", {{"app", foreground_->appName()}});
    rt_.view().popToRoot();
    paused_ = std::move(foreground_);
}

void AppHostManager::resumePaused() {
    if (!paused_) return;
    rt_.log().info("AppHostManager", "resume", {{"app", paused_->appName()}});
    foreground_ = std::move(paused_);
    rt_.view().push(*foreground_);
}

void AppHostManager::killPaused() {
    if (!paused_) return;
    rt_.log().info("AppHostManager", "killPaused", {{"app", paused_->appName()}});
    paused_->requestExit();
    paused_.reset();
}

const char* AppHostManager::pausedName() const {
    return paused_ ? paused_->appName() : nullptr;
}

const char* AppHostManager::foregroundName() const {
    return foreground_ ? foreground_->appName() : "";
}

bool AppHostManager::isFailed(const std::string& appId) const {
    auto it = crashMap_.find(appId);
    return it != crashMap_.end() && it->second.crashCount >= kMaxCrashes;
}

void AppHostManager::onAppExited(const Event& e) {
    std::string appId, appName;
    int exitCode = 0;
    for (const auto& f : e.payload) {
        if (std::string(f.key) == "id") appId = f.value;
        if (std::string(f.key) == "name") appName = f.value;
        if (std::string(f.key) == "exitCode") exitCode = std::stoi(f.value);
    }
    if (appId.empty()) return;
    if (exitCode == 0) {
        crashMap_.erase(appId);
        return;
    }

    auto& rec = crashMap_[appId];
    uint64_t now = rt_.clock().millis();

    if (rec.firstCrashMs == 0 || (now - rec.firstCrashMs) > kCrashWindowMs) {
        rec.crashCount = 0;
        rec.firstCrashMs = now;
    }
    rec.crashCount++;

    rt_.log().error("AppHostManager", "crash",
        {{"app", appName}, {"count", std::to_string(rec.crashCount)}, {"max", std::to_string(kMaxCrashes)}});

    if (rec.crashCount >= kMaxCrashes) {
        rt_.log().error("AppHostManager", "app marked FAILED",
            {{"app", appName}, {"crashes", std::to_string(rec.crashCount)}});
        return;
    }

    IApp* app = rt_.apps().getApp(appId.c_str());
    if (!app) return;

    uint32_t delayMs = 0;
    if (rec.crashCount == 2) delayMs = 5000;
    if (rec.crashCount == 3) delayMs = 30000;

    if (delayMs == 0) {
        rt_.log().info("AppHostManager", "restart immediate", {{"app", appName}});
        doLaunch(*app);
    } else {
        rt_.log().info("AppHostManager", "restart scheduled",
            {{"app", appName}, {"delayMs", std::to_string(delayMs)}});
        pendingRestart_ = std::make_unique<PendingRestart>();
        pendingRestart_->appId = appId;
        pendingRestart_->restartAtMs = now + delayMs;
        pendingRestart_->attempt = rec.crashCount;
    }
}

void AppHostManager::onClockTick(const Event&) {
    if (!pendingRestart_) return;
    uint64_t now = rt_.clock().millis();
    if (now < pendingRestart_->restartAtMs) return;

    auto pr = std::move(pendingRestart_);
    IApp* app = rt_.apps().getApp(pr->appId.c_str());
    if (app) {
        rt_.log().info("AppHostManager", "restart now",
            {{"app", app->name()}, {"attempt", std::to_string(pr->attempt)}});
        doLaunch(*app);
    }
}

} // namespace nema
