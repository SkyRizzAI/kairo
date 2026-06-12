#include "nema/sim/sim_wifi_driver.h"
#include "nema/log/logger.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include "nema/event/async_event_poster.h"
#include "nema/thread.h"
#include <cstring>

namespace nema {

void SimWifiDriver::init(Logger& log, EventBus& events, AsyncEventPoster* poster) {
    log_    = &log;
    events_ = &events;
    poster_ = poster;
    // Default "router" environment so the device works out of the box.
    nets_ = {
        {"MyHomeWiFi",      "password123", -42, true},
        {"CoffeeShop_Free", "",            -68, true},
        {"Neighbour_5G",    "secret",      -74, false},  // reachable but offline
        {"AndroidAP",       "hotspot00",   -81, true},
    };
}

void SimWifiDriver::start() { log_->info("SimWifiDriver", "started"); }

void SimWifiDriver::stop() {
    if (connected_) disconnect();
    log_->info("SimWifiDriver", "stopped");
}

void SimWifiDriver::setNetworks(std::vector<SimNet> nets) {
    nets_ = std::move(nets);
    log_->info("SimWifiDriver", "router networks updated",
               {{"count", std::to_string(nets_.size())}});
}

const SimWifiDriver::SimNet* SimWifiDriver::findNet(const std::string& ssid) const {
    for (auto& n : nets_) if (n.ssid == ssid) return &n;
    return nullptr;
}

bool SimWifiDriver::isOnline() const {
    if (!connected_) return false;
    const SimNet* n = findNet(ssid_);
    return n && n->online;
}

bool SimWifiDriver::connect(const char* ssid, const char* password) {
    const SimNet* n = findNet(ssid);
    if (!n) {                                  // SSID not in range
        log_->warn("SimWifiDriver", "ssid not found", {{"ssid", ssid}});
        connected_ = false;
        return false;
    }
    if (!n->password.empty() && n->password != (password ? password : "")) {
        log_->warn("SimWifiDriver", "auth failed (wrong password)", {{"ssid", ssid}});
        connected_ = false;
        return false;
    }
    ssid_      = ssid;
    connected_ = true;
    if (ip_ == "0.0.0.0") ip_ = "192.168.1.123";
    log_->info("SimWifiDriver", "connected",
               {{"ssid", ssid}, {"online", n->online ? "yes" : "no"}});
    events_->publish({events::NetworkConnected, {{"ssid", ssid}}});
    return true;
}

void SimWifiDriver::disconnect() {
    auto prev = ssid_;
    ssid_      = "";
    connected_ = false;
    log_->info("SimWifiDriver", "disconnected", {{"ssid", prev}});
    events_->publish({events::NetworkDisconnected, {{"ssid", prev}}});
}

void SimWifiDriver::scan() {
    // Worker thread. Mimic radio latency so "Scanning..." is visible.
    nema::Thread::sleepMs(800);
    scan_.clear();
    for (auto& n : nets_) {
        WifiNetwork w{};
        std::strncpy(w.ssid, n.ssid.c_str(), sizeof(w.ssid) - 1);
        w.rssi    = n.rssi;
        w.secured = !n.password.empty();
        scan_.push_back(w);
    }
    if (poster_)
        poster_->post({events::WifiScanComplete,
                       {{"count", std::to_string(scan_.size())}}});
    log_->info("SimWifiDriver", "scan complete",
               {{"count", std::to_string(scan_.size())}});
}

void SimWifiDriver::tick(uint64_t /*nowMs*/) {}

} // namespace nema
