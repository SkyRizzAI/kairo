#pragma once
#include "nema/types.h"
#include <vector>

namespace nema {

struct Event {
    const char*        name;
    std::vector<Field> payload;
};

namespace events {
    inline constexpr const char* SystemBoot           = "SystemBoot";
    inline constexpr const char* SystemReady          = "SystemReady";
    inline constexpr const char* ServiceStarted       = "ServiceStarted";
    inline constexpr const char* ServiceStopped       = "ServiceStopped";
    inline constexpr const char* ServiceFailed        = "ServiceFailed";
    inline constexpr const char* BatteryChanged       = "BatteryChanged";
    inline constexpr const char* NetworkConnected     = "NetworkConnected";
    inline constexpr const char* NetworkDisconnected  = "NetworkDisconnected";
    // Resource liveness (Plan 42): a single uniform event for every resource.
    // payload: {{"resource","<cap>"}, {"state","available"|"absent"|"fault"}}
    inline constexpr const char* ResourceChanged      = "ResourceChanged";
    inline constexpr const char* WifiScanComplete     = "WifiScanComplete";   // {"count":"N"}
    inline constexpr const char* ClockTick            = "ClockTick";
    // Bluetooth/BLE (Plan 34)
    inline constexpr const char* BtEnabled            = "BtEnabled";
    inline constexpr const char* BtDisabled           = "BtDisabled";
    inline constexpr const char* BtPairRequest        = "BtPairRequest";       // {"passkey":"483921"}
    inline constexpr const char* BtPaired             = "BtPaired";            // {"name":"iPhone"}
    inline constexpr const char* BtConnected          = "BtConnected";         // {"name":"iPhone"}
    inline constexpr const char* BtDisconnected       = "BtDisconnected";
    // App registry (install/remove a launchable app — built-in or custom)
    inline constexpr const char* AppInstalled         = "AppInstalled";        // {"id","name"}
    inline constexpr const char* AppRemoved           = "AppRemoved";          // {"id"}
    // AppHost lifecycle — emitted when an app thread exits (clean or crash).
    // payload: {{"id","<app-id>"}, {"name","<app-name>"}, {"exitCode","<int>"}}
    inline constexpr const char* AppHostExited        = "AppHostExited";
    // Non-MVP (declared for future use)
    inline constexpr const char* NotificationCreated  = "NotificationCreated";
}

} // namespace nema
