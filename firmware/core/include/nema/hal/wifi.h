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
    char dns[16]  = {};
};

// A saved network profile (the password lives in the driver's secure store,
// never exposed here). POD — safe to copy across threads.
struct WifiProfile {
    char ssid[33]  = {};
    bool secured   = false;
    bool autoJoin  = true;   // join automatically when in range
};

// Connection lifecycle — what the UI/remote shows instead of a bare bool.
enum class WifiState : uint8_t {
    Disabled,    // radio off / driver not started
    Idle,        // on, not connected
    Scanning,    // a scan() is in flight
    Connecting,  // association / auth / DHCP in flight
    Connected,   // associated and has an IP
    Failed,      // last attempt failed — see lastError()
};

// Why the last connect attempt failed.
enum class WifiError : uint8_t {
    None,
    AuthFailed,   // wrong password / auth rejected
    ApNotFound,   // SSID not in range
    Timeout,
    DhcpFailed,
    Unknown,
};

// WiFi station driver.
//
// Threading contract (Nema): scan()/connect()/autoConnect() may BLOCK and are
// invoked from a TaskRunner worker thread. The query methods
// (state/lastError/rssi/isConnected/ssid/ip/ipConfig/scanResults/saved*) are
// read from the UI thread; drivers update state via the AsyncEventPoster so the
// happens-before is the event drain. Callers honour the state machine (don't
// read scanResults() mid-scan).
struct IWifiDriver : IDriver {
    // ── radio power (Wi-Fi on/off, iOS-style) ──
    // Default: derive "on" from state; setEnabled is a no-op for drivers that
    // can't toggle the radio. esp32 stops/starts the station.
    virtual bool isEnabled() const { return state() != WifiState::Disabled; }
    virtual void setEnabled(bool /*on*/) {}

    // ── connection ──
    virtual bool        connect(const char* ssid, const char* password = "") = 0;
    // Reconnect to a saved network using its stored credentials (no re-prompt).
    virtual bool        connectSaved(const char* ssid) { (void)ssid; return false; }
    virtual void        disconnect()   = 0;
    virtual bool        isConnected()  const = 0;
    virtual const char* ssid()         const = 0;

    // ── status (UI thread, non-blocking) ──
    virtual WifiState state()     const = 0;
    virtual WifiError lastError() const = 0;
    virtual int8_t    rssi()      const = 0;   // RSSI of the connected AP, 0 if none
    // Single source of truth for "has IP connectivity" — gate HTTP/NTP/remote-TCP
    // here, not on isConnected(), so other link types can satisfy it later.
    virtual bool      isOnline()  const { return isConnected(); }

    // ── scan (blocking, worker thread) ──
    virtual void                            scan() = 0;
    virtual const std::vector<WifiNetwork>& scanResults() const = 0;

    // ── IPv4 ──
    virtual const char*  ip()       const = 0;
    virtual WifiIpConfig ipConfig() const = 0;
    virtual void         setIpConfig(const WifiIpConfig&) = 0;

    // ── saved networks (optional for minimal drivers; default = none) ──
    // Persisted credentials so the device reconnects without re-typing.
    virtual void   saveNetwork(const char* ssid, const char* password) { (void)ssid; (void)password; }
    virtual void   forgetNetwork(const char* ssid) { (void)ssid; }
    virtual void   setAutoJoin(const char* ssid, bool on) { (void)ssid; (void)on; }
    virtual size_t savedCount() const { return 0; }
    virtual bool   savedAt(size_t i, WifiProfile& out) const { (void)i; (void)out; return false; }
    // Pick the strongest saved network in range and connect (blocking, worker).
    virtual void   autoConnect() {}
};

// Stable short names for event payloads / UI labels.
inline const char* wifiStateName(WifiState s) {
    switch (s) {
        case WifiState::Disabled:   return "disabled";
        case WifiState::Idle:       return "idle";
        case WifiState::Scanning:   return "scanning";
        case WifiState::Connecting: return "connecting";
        case WifiState::Connected:  return "connected";
        case WifiState::Failed:     return "failed";
    }
    return "idle";
}
inline const char* wifiErrorName(WifiError e) {
    switch (e) {
        case WifiError::None:       return "none";
        case WifiError::AuthFailed: return "auth";
        case WifiError::ApNotFound: return "notfound";
        case WifiError::Timeout:    return "timeout";
        case WifiError::DhcpFailed: return "dhcp";
        case WifiError::Unknown:    return "unknown";
    }
    return "none";
}

} // namespace nema
