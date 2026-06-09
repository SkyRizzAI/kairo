#include "kairo/app/app_host_manager.h"
#include "kairo/app/app_host.h"
#include "kairo/app/app.h"
#include "kairo/screens/close_and_open_modal.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/ui/view_dispatcher.h"

namespace kairo {

AppHostManager::AppHostManager(Runtime& rt) : rt_(rt) {}
AppHostManager::~AppHostManager() = default;

void AppHostManager::doLaunch(IApp& app) {
    foreground_.reset();   // free the previous (now-finished) host
    foreground_ = std::make_unique<AppHost>(rt_, app);
    rt_.log().info("AppHostManager", "launch", {{"app", app.name()}});
    rt_.view().push(*foreground_);
}

void AppHostManager::launch(IApp& app) {
    if (paused_) {
        // Single-slot policy: confirm before discarding the backgrounded app.
        modal_ = std::make_unique<CloseAndOpenModal>(rt_, *this, app);
        rt_.view().push(*modal_);
        return;
    }
    doLaunch(app);
}

void AppHostManager::pauseForeground() {
    // Only pause if the foreground app is actually the active (top) screen.
    if (!foreground_ || rt_.view().active() != foreground_.get()) return;
    foreground_->setPaused(true);
    rt_.log().info("AppHostManager", "pause", {{"app", foreground_->appName()}});
    rt_.view().popToRoot();           // back to Home (drops app + any launcher above)
    paused_ = std::move(foreground_); // keep it alive (thread parked)
}

void AppHostManager::resumePaused() {
    if (!paused_) return;
    rt_.log().info("AppHostManager", "resume", {{"app", paused_->appName()}});
    foreground_ = std::move(paused_);
    rt_.view().push(*foreground_);    // enter() clears the pause flag
}

void AppHostManager::killPaused() {
    if (!paused_) return;
    rt_.log().info("AppHostManager", "killPaused", {{"app", paused_->appName()}});
    paused_->requestExit();           // also clears pause → parked thread returns
    paused_.reset();                  // dtor joins the app thread
}

const char* AppHostManager::pausedName() const {
    return paused_ ? paused_->appName() : nullptr;
}

} // namespace kairo
