#include "kairo/sim/bridge.h"
#include "kairo/sim/sim_wifi_driver.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/event/event_bus.h"
#include "kairo/event/event.h"
#include "kairo/service/service_container.h"
#include "kairo/ui/key.h"
#include <nlohmann/json.hpp>
#include <poll.h>
#include <unistd.h>
#include <string>
#include <cstring>

namespace kairo {

void CommandReader::init(Runtime& rt) {
    rt_ = &rt;
}

void CommandReader::poll() {
    struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
    if (::poll(&pfd, 1, 0) <= 0) return;

    char buf[4096];
    ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (n <= 0) { rt_->requestShutdown(); return; }  // EOF → shutdown

    buf[n] = '\0';
    lineBuf_ += buf;

    size_t nl;
    while ((nl = lineBuf_.find('\n')) != std::string::npos) {
        std::string line = lineBuf_.substr(0, nl);
        lineBuf_ = lineBuf_.substr(nl + 1);
        if (!line.empty()) dispatch(line);
    }
}

void CommandReader::dispatch(const std::string& line) {
    auto j = nlohmann::json::parse(line, nullptr, false);
    if (j.is_discarded()) {
        rt_->log().warn("CommandReader", "bad JSON command: " + line);
        return;
    }

    std::string cmd = j.value("cmd", "");

    if (cmd == "shutdown") {
        rt_->requestShutdown();

    } else if (cmd == "restart") {
        rt_->requestRestart();

    } else if (cmd == "inject_event") {
        std::string evtName = j.value("name", "");
        if (evtName.empty()) return;
        std::vector<Field> payload;
        if (j.contains("payload") && j["payload"].is_object()) {
            for (auto& [k, v] : j["payload"].items()) {
                payload.push_back({k.c_str(), v.is_string() ? v.get<std::string>() : v.dump()});
            }
        }
        // name must live as long as the publish call (stack string → c_str() is fine here)
        rt_->events().publish({evtName.c_str(), std::move(payload)});

    } else if (cmd == "wifi_connect") {
        std::string ssid = j.value("ssid", "");
        std::string pass = j.value("password", "");
        auto* wifi = rt_->container().resolve<SimWifiDriver>();
        if (wifi) wifi->connect(ssid.c_str(), pass.c_str());
        else rt_->log().warn("CommandReader", "wifi driver not found");

    } else if (cmd == "wifi_disconnect") {
        auto* wifi = rt_->container().resolve<SimWifiDriver>();
        if (wifi) wifi->disconnect();

    } else if (cmd == "wifi_set_networks") {
        // Web "router" panel sets the full list of nearby networks. The device
        // scans/picks/types the password itself, like real hardware.
        auto* wifi = rt_->container().resolve<SimWifiDriver>();
        if (wifi && j.contains("networks") && j["networks"].is_array()) {
            std::vector<SimWifiDriver::SimNet> nets;
            for (auto& n : j["networks"]) {
                SimWifiDriver::SimNet w;
                w.ssid     = n.value("ssid", "");
                w.password = n.value("password", "");
                w.rssi     = (int8_t)n.value("rssi", -60);
                w.online   = n.value("online", true);
                if (!w.ssid.empty()) nets.push_back(std::move(w));
            }
            wifi->setNetworks(std::move(nets));
        }

    } else if (cmd == "press_key") {
        std::string keyStr = j.value("key", "");
        Key k = keyFromName(keyStr.c_str());
        // Funnel through InputService (same path as hardware) — Runtime::step() drains it.
        if (k != Key::None) rt_->input().post(k);

    } else {
        rt_->log().warn("CommandReader", "unknown command", {{"cmd", cmd}});
    }
}

} // namespace kairo
