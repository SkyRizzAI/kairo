#pragma once
#include "nema/hal/driver.h"
#include <vector>
#include <cstdint>

namespace nema {

// One scan result. POD — safe to copy across threads.
struct WifiNetwork {
    char    ssid[33] = {};
    int8_t  rssi     = 0;     // dBm, e.g. -65
    bool    secured  = false; // needs a password
};

// IPv4 configuration. Strings are null-terminated dotted-quad.
struct WifiIpConfig {
    bool dhcp = true;
    char ip[16]   = {};
    char mask[16] = {};
    char gw[16]   = {};
};

// WiFi station driver.
//
// Threading contract (Nema): scan()/connect() may BLOCK and are invoked from a
// TaskRunner worker thread. scanResults()/ip()/isConnected()/ssid()/ipConfig()
// are read from the app thread AFTER the worker's completion callback fires
// (happens-before via the task queue) — so no internal lock is required as long
// as callers honour the state machine (don't read results mid-scan).
struct IWifiDriver : IDriver {
    virtual bool        connect(const char* ssid, const char* password = "") = 0;
    virtual void        disconnect()   = 0;
    virtual bool        isConnected()  const = 0;
    virtual const char* ssid()         const = 0;

    // Blocking scan (worker thread). Fills scanResults().
    virtual void                            scan() = 0;
    virtual const std::vector<WifiNetwork>& scanResults() const = 0;

    virtual const char*  ip()       const = 0;
    virtual WifiIpConfig ipConfig() const = 0;
    virtual void         setIpConfig(const WifiIpConfig&) = 0;
};

} // namespace nema
