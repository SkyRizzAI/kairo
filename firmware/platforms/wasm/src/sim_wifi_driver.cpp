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

// Post a state transition (worker-safe via poster_, else synchronous bus).
static void emitState(EventBus* bus, AsyncEventPoster* poster,
                      WifiState s, WifiError e) {
    Event ev{events::WifiStateChanged,
             {{"state", wifiStateName(s)}, {"err", wifiErrorName(e)}}};
    if (poster) poster->post(ev);
    else if (bus) bus->publish(ev);
}

int8_t SimWifiDriver::rssi() const {
    if (!connected_) return 0;
    const SimNet* n = findNet(ssid_);
    return n ? n->rssi : 0;
}

bool SimWifiDriver::connect(const char* ssid, const char* password) {
    const SimNet* n = findNet(ssid);
    if (!n) {                                  // SSID not in range
        log_->warn("SimWifiDriver", "ssid not found", {{"ssid", ssid}});
        connected_ = false;
        state_ = WifiState::Failed; lastError_ = WifiError::ApNotFound;
        emitState(events_, poster_, state_, lastError_);
        return false;
    }
    if (!n->password.empty() && n->password != (password ? password : "")) {
        log_->warn("SimWifiDriver", "auth failed (wrong password)", {{"ssid", ssid}});
        connected_ = false;
        state_ = WifiState::Failed; lastError_ = WifiError::AuthFailed;
        emitState(events_, poster_, state_, lastError_);
        return false;
    }
    ssid_      = ssid;
    connected_ = true;
    state_ = WifiState::Connected; lastError_ = WifiError::None;
    if (ip_ == "0.0.0.0") ip_ = "192.168.1.123";
    if (ipcfg_.dhcp) {   // populate displayable IPv4 info (DHCP "lease")
        std::strncpy(ipcfg_.ip,   "192.168.1.123", sizeof(ipcfg_.ip) - 1);
        std::strncpy(ipcfg_.mask, "255.255.255.0", sizeof(ipcfg_.mask) - 1);
        std::strncpy(ipcfg_.gw,   "192.168.1.1",   sizeof(ipcfg_.gw) - 1);
        std::strncpy(ipcfg_.dns,  "192.168.1.1",   sizeof(ipcfg_.dns) - 1);
    }
    saveNetwork(ssid, password);
    log_->info("SimWifiDriver", "connected",
               {{"ssid", ssid}, {"online", n->online ? "yes" : "no"}});
    emitState(events_, poster_, state_, lastError_);
    if (poster_) poster_->post({events::NetworkConnected, {{"ssid", ssid}}});
    else events_->publish({events::NetworkConnected, {{"ssid", ssid}}});
    return true;
}

void SimWifiDriver::disconnect() {
    auto prev = ssid_;
    ssid_      = "";
    connected_ = false;
    state_ = WifiState::Idle; lastError_ = WifiError::None;
    log_->info("SimWifiDriver", "disconnected", {{"ssid", prev}});
    emitState(events_, poster_, state_, lastError_);
    if (poster_) poster_->post({events::NetworkDisconnected, {{"ssid", prev}}});
    else events_->publish({events::NetworkDisconnected, {{"ssid", prev}}});
}

void SimWifiDriver::setEnabled(bool on) {
    if (on == enabled_) return;
    enabled_ = on;
    if (!on) {
        if (connected_) disconnect();
        state_ = WifiState::Disabled;
        emitState(events_, poster_, state_, lastError_);
    } else {
        state_ = WifiState::Idle;
        emitState(events_, poster_, state_, lastError_);
        autoConnect();
    }
}

bool SimWifiDriver::connectSaved(const char* ssid) {
    for (auto& sp : saved_)
        if (sp.ssid == ssid) return connect(sp.ssid.c_str(), sp.pass.c_str());
    return false;
}

void SimWifiDriver::saveNetwork(const char* ssid, const char* password) {
    if (!ssid || !ssid[0]) return;
    for (auto it = saved_.begin(); it != saved_.end(); ++it)
        if (it->ssid == ssid) { saved_.erase(it); break; }
    saved_.insert(saved_.begin(), {ssid, password ? password : ""});
    if (saved_.size() > 4) saved_.resize(4);
}

void SimWifiDriver::forgetNetwork(const char* ssid) {
    for (auto it = saved_.begin(); it != saved_.end(); ++it)
        if (it->ssid == ssid) { saved_.erase(it); return; }
}

void SimWifiDriver::setAutoJoin(const char* ssid, bool on) {
    for (auto& sp : saved_) if (sp.ssid == ssid) sp.autojoin = on;
}

bool SimWifiDriver::savedAt(size_t i, WifiProfile& out) const {
    if (i >= saved_.size()) return false;
    std::strncpy(out.ssid, saved_[i].ssid.c_str(), sizeof(out.ssid) - 1);
    out.secured  = !saved_[i].pass.empty();
    out.autoJoin = saved_[i].autojoin;
    return true;
}

void SimWifiDriver::autoConnect() {
    if (saved_.empty()) return;
    scan();
    const WifiNetwork* best = nullptr;
    const SavedProfile* bestSaved = nullptr;
    for (auto& ap : scan_)
        for (auto& sp : saved_) {
            if (!sp.autojoin) continue;
            if (sp.ssid == ap.ssid && (!best || ap.rssi > best->rssi)) { best = &ap; bestSaved = &sp; }
        }
    if (bestSaved) connect(bestSaved->ssid.c_str(), bestSaved->pass.c_str());
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
