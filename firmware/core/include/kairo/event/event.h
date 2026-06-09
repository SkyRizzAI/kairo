#pragma once
#include "kairo/types.h"
#include <vector>

namespace kairo {

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
    inline constexpr const char* WifiScanComplete     = "WifiScanComplete";   // {"count":"N"}
    inline constexpr const char* ClockTick            = "ClockTick";
    // Bluetooth/BLE (Plan 34)
    inline constexpr const char* BtEnabled            = "BtEnabled";
    inline constexpr const char* BtDisabled           = "BtDisabled";
    inline constexpr const char* BtPairRequest        = "BtPairRequest";       // {"passkey":"483921"}
    inline constexpr const char* BtPaired             = "BtPaired";            // {"name":"iPhone"}
    inline constexpr const char* BtConnected          = "BtConnected";         // {"name":"iPhone"}
    inline constexpr const char* BtDisconnected       = "BtDisconnected";
    // Non-MVP (declared for future use)
    inline constexpr const char* PluginLoaded         = "PluginLoaded";
    inline constexpr const char* PluginUnloaded       = "PluginUnloaded";
    inline constexpr const char* NotificationCreated  = "NotificationCreated";
}

} // namespace kairo
