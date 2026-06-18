#include "nema/services/ntp_service.h"
#include "nema/runtime.h"
#include "nema/platform.h"
#include "nema/clock.h"
#include "nema/log/logger.h"
#include "nema/event/event.h"
#include "nema/event/event_bus.h"
#include "nema/task_runner.h"

namespace nema {

void NtpService::start() {
    rt_.events().subscribe(events::NetworkConnected,
        [this](const Event&) {
            if (!syncing_) syncNow();
        });
    rt_.log().info("NtpService", "started");
}

void NtpService::tick(uint64_t nowMs) {
    if (lastSyncMs_ == 0 || syncing_) return;
    if (nowMs - lastSyncMs_ < 86400000) return;  // 24h
    rt_.log().info("NtpService", "periodic resync");
    syncNow();
}

void NtpService::syncNow() {
    if (syncing_) return;
    syncing_ = true;
    rt_.tasks().submit(
        [this] { doSync(); },
        [this] { syncing_ = false; });
}

void NtpService::doSync() {
    bool ok = rt_.platform().syncNtp();
    if (ok) {
        lastSyncMs_ = rt_.clock().millis();
        rt_.log().info("NtpService", "synced",
            {{"epoch", std::to_string(rt_.clock().epochMs())}});
    } else {
        rt_.log().warn("NtpService", "sync failed");
    }
}

} // namespace nema
