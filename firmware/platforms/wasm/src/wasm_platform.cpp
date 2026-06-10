#include "kairo/wasm/wasm_platform.h"
#include "kairo/runtime.h"
#include "kairo/service/service_container.h"
#include "kairo/services/input_service.h"
#include "kairo/system/hardware_registry.h"
#include "kairo/system/capability_registry.h"
#include "kairo/board.h"
#include "kairo/hal/display.h"
#include "kairo/hal/wifi.h"
#include "kairo/config/config_store.h"
#include "kairo/event/event_bus.h"
#include "kairo/plugins/js_app_store.h"
#include <string>
#include <vector>
#include <cstdlib>

namespace kairo {

void WasmPlatform::registerDrivers(Runtime& rt) {
    rt_ = &rt;

    cable_.init();
    link_.attach(&cable_, LinkService::Role::Device);
    tap_.init(display_, link_);

    rt.container().registerService(&display_);          // NullDisplay (IService)
    rt.container().registerAs<IDisplayDriver>(&tap_);    // Canvas renders into the tap → streamed
    rt.container().registerAs<IConfigStore>(&config_);

    // WiFi (virtual router) — same SimWifiDriver as the native sim, so the
    // device's WiFi app works and Forge's WiFi panel can inject networks.
    wifi_.init(rt.log(), rt.events(), &rt.asyncPoster());
    rt.container().registerService(&wifi_);
    rt.container().registerAs<IWifiDriver>(&wifi_);
    rt.hardware().add({"wifi", DriverKind::Wifi, "virtual"});
    rt.capabilities().add("wifi");
    rt.capabilities().add("networking");

    remote_.init(link_, rt.input());
    remote_.attachLog(rt.log());
    remote_.attachEvents(rt.events());                // stream events → EVENT channel
    remote_.onPower(&WasmPlatform::powerThunk, this);
    remote_.onControl(&WasmPlatform::controlThunk, this);
    remote_.setProfile(rt.board().profile());
    link_.onReady(&WasmPlatform::readyThunk, this);   // push current screen on connect

    rt.hardware().add({"display", DriverKind::Display, "wasm 1-bit (remote)"});
    rt.capabilities().add("display");
    rt.capabilities().add("input");
}

void WasmPlatform::controlThunk(void* user, uint8_t op, const uint8_t* data, size_t len) {
    auto* s = static_cast<WasmPlatform*>(user);
    if (op == ExtOp::AppInstall) {   // OTA: Forge pushed a .kapp → install live (Plan 37)
        if (s->rt_) JsAppStore::instance().installKapp(*s->rt_, (const char*)data, len);
        return;
    }
    if (op != ExtOp::WifiSetNetworks) return;
    // data: "ssid\tpw\trssi\tonline\n..." (one network per line)
    std::string blob((const char*)data, len);
    std::vector<SimWifiDriver::SimNet> nets;
    size_t pos = 0;
    while (pos < blob.size()) {
        size_t nl = blob.find('\n', pos);
        std::string line = blob.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? blob.size() : nl + 1;
        if (line.empty()) continue;
        SimWifiDriver::SimNet n;
        size_t a = line.find('\t'), b = line.find('\t', a + 1), c = line.find('\t', b + 1);
        if (a == std::string::npos || b == std::string::npos || c == std::string::npos) continue;
        n.ssid = line.substr(0, a);
        n.password = line.substr(a + 1, b - a - 1);
        n.rssi = (int8_t)std::atoi(line.substr(b + 1, c - b - 1).c_str());
        n.online = line.substr(c + 1) != "0";
        if (!n.ssid.empty()) nets.push_back(std::move(n));
    }
    s->wifi_.setNetworks(std::move(nets));
}

void WasmPlatform::readyThunk(void* user) {
    static_cast<WasmPlatform*>(user)->tap_.requestResend();
}

void WasmPlatform::powerThunk(void* user, uint8_t op) {
    auto* s = static_cast<WasmPlatform*>(user);
    if (!s->rt_) return;
    if (op == SysOp::Restart)       s->rt_->requestRestart();
    else if (op == SysOp::Shutdown) s->rt_->requestShutdown();
}

} // namespace kairo
