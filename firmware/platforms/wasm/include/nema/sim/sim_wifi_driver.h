#pragma once
#include "nema/hal/wifi.h"
#include "nema/service.h"
#include <string>
#include <vector>

namespace nema {

class Logger;
class EventBus;
class AsyncEventPoster;

// Simulated WiFi backed by an interactive "router" model driven from the web
// panel. Each network has a correct password and an online flag (whether it
// reaches the internet). The device runs the real flow — scan → pick SSID →
// type password → connect — and connect() validates the password like a router.
// HTTP only works when the connected network is online, so toggling a network
// offline in the panel makes networked apps fail exactly like real hardware.
class SimWifiDriver : public IWifiDriver, public IService {
public:
    struct SimNet {
        std::string ssid;
        std::string password;   // empty = open network
        int8_t      rssi   = -60;
        bool        online = true;   // has internet?
    };

    void init(Logger& log, EventBus& events, AsyncEventPoster* poster = nullptr);

    // IDriver
    const char* name() const override { return "SimWifiDriver"; }
    DriverKind  kind() const override { return DriverKind::Wifi; }

    // IWifiDriver
    bool connect(const char* ssid, const char* password = "") override;
    void disconnect() override;
    bool isConnected() const override { return connected_; }
    const char* ssid() const override { return ssid_.c_str(); }

    void                            scan() override;
    const std::vector<WifiNetwork>& scanResults() const override { return scan_; }
    const char*  ip()       const override { return ip_.c_str(); }
    WifiIpConfig ipConfig() const override { return ipcfg_; }
    void         setIpConfig(const WifiIpConfig& c) override { ipcfg_ = c; }

    // Web "router" panel sets the full list of nearby networks.
    void setNetworks(std::vector<SimNet> nets);
    // True only when connected AND that network is online — gates HTTP.
    bool isOnline() const;

    // IService
    void start() override;
    void stop()  override;
    void tick(uint64_t nowMs) override;

private:
    const SimNet* findNet(const std::string& ssid) const;

    Logger*           log_     = nullptr;
    EventBus*         events_  = nullptr;
    AsyncEventPoster* poster_  = nullptr;
    bool              connected_ = false;
    std::string       ssid_;
    std::string       ip_ = "0.0.0.0";
    WifiIpConfig      ipcfg_;

    std::vector<SimNet>      nets_;    // the router's known networks
    std::vector<WifiNetwork> scan_;    // last scan() projection
};

} // namespace nema
