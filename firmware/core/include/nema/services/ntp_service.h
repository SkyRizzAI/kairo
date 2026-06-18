#pragma once
#include "nema/service.h"
#include <cstdint>

namespace nema {

class Runtime;
class EventBus;

// NtpService — auto-syncs the device clock after WiFi connects.
//
// Subscribes to NetworkConnected → submits a blocking SNTP sync to the
// TaskRunner worker → sets rt.clock().setEpochMs() on completion (UI thread).
// Re-syncs every 24h while online.
class NtpService : public IService {
public:
    explicit NtpService(Runtime& rt) : rt_(rt) {}

    const char* name() const override { return "NtpService"; }
    void start() override;
    void stop() override {};
    void tick(uint64_t nowMs) override;

    // Force an immediate sync (called from Settings or CLI).
    void syncNow();

private:
    void doSync();

    Runtime&    rt_;
    uint64_t    lastSyncMs_ = 0;
    bool        syncing_ = false;
};

} // namespace nema
