#pragma once
#include "nema/service.h"
#include <cstdint>

namespace nema {

class Runtime;

// SystemWifiManager — coordinates the system's managed WiFi lease with the
// ResourceBroker (Plan 87 Fase 3).
//
// At startup, the system is considered the "soft" holder of net.wifi.managed.
// When the device connects (NetworkConnected), it acquires that lease as
// "system:wifi".  When an app acquires net.wifi.monitor or net.wifi.inject,
// the broker auto-yields "system:wifi" and emits ResourceSuspended — this
// service catches it and calls wifi->disconnect() so the radio is free for the
// app. When the app releases, ResourceRestored is emitted — this service
// re-acquires the lease and calls wifi->autoConnect() to restore connectivity.
//
// OTA guard: the OtaUpdater acquires "net.wifi.managed" as "system:ota" before
// downloading. Because "system:ota" is NOT the yieldableOwner, any app trying
// to acquire monitor/inject during OTA gets LeaseError{busy, "system:ota"}.
class SystemWifiManager : public IService {
public:
    void init(Runtime& rt);

    const char* name() const override { return "SystemWifiManager"; }
    void start() override {}
    void stop()  override {}

private:
    uint32_t leaseHandle_ = 0;
    Runtime* rt_ = nullptr;
};

} // namespace nema
