#pragma once
#include "nema/hal/wifi.h"
#include <cstring>
#include <vector>

namespace nema {

// NullWifiDriver — the smallest CONFORMING IWifiDriver (RAM-only, one fake open
// AP "TestAP"). Two jobs:
//   1. Reference for board authors: it overrides ONLY the mandatory methods and
//      leaves the optional ones (saved networks / auto-join / radio power / IP
//      config) on their HAL defaults — i.e. the minimum a new board must write.
//   2. Backs wifi_contract_test on the host (which has no real radio).
class NullWifiDriver : public IWifiDriver {
public:
    const char* name() const override { return "NullWifiDriver"; }
    DriverKind  kind() const override { return DriverKind::Wifi; }

    bool connect(const char* ssid, const char* /*password*/ = "") override {
        state_ = WifiState::Connecting;
        if (ssid && std::strcmp(ssid, "TestAP") == 0) {
            std::strncpy(ssid_, ssid, sizeof(ssid_) - 1);
            connected_ = true;
            state_     = WifiState::Connected;
            lastError_ = WifiError::None;
            return true;
        }
        connected_ = false;
        state_     = WifiState::Failed;
        lastError_ = WifiError::ApNotFound;
        return false;
    }
    void disconnect() override {
        connected_ = false;
        ssid_[0]   = '\0';
        state_     = WifiState::Idle;
    }
    bool        isConnected() const override { return connected_; }
    const char* ssid()        const override { return ssid_; }

    WifiState state()     const override { return state_; }
    WifiError lastError() const override { return lastError_; }
    int8_t    rssi()      const override { return connected_ ? -50 : 0; }

    void scan() override {
        scan_.clear();
        WifiNetwork w{};
        std::strncpy(w.ssid, "TestAP", sizeof(w.ssid) - 1);
        w.rssi    = -50;
        w.secured = false;
        scan_.push_back(w);
    }
    const std::vector<WifiNetwork>& scanResults() const override { return scan_; }
    const char*  ip()       const override { return connected_ ? "10.0.0.2" : "0.0.0.0"; }
    WifiIpConfig ipConfig() const override { return ipcfg_; }
    void         setIpConfig(const WifiIpConfig& c) override { ipcfg_ = c; }

private:
    bool         connected_ = false;
    WifiState    state_      = WifiState::Idle;
    WifiError    lastError_  = WifiError::None;
    char         ssid_[33]   = {};
    WifiIpConfig ipcfg_;
    std::vector<WifiNetwork> scan_;
};

} // namespace nema
