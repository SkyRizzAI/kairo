#include "kairo/sim/simulator_platform.h"
#include "kairo/sim/bridge.h"
#include "kairo/runtime.h"
#include "kairo/service/service_container.h"
#include "kairo/system/hardware_registry.h"
#include "kairo/system/capability_registry.h"
#include "kairo/hal/display.h"
#include "kairo/hal/http_client.h"
#include "kairo/config/config_store.h"
#include <cstdlib>
#include <thread>
#include <chrono>

namespace kairo {

SimulatorPlatform::SimulatorPlatform()
    : cmdReader_(std::make_unique<CommandReader>())
    , bridge_(std::make_unique<TelemetryBridge>()) {
    const char* env = std::getenv("KAIRO_SIM_JSON");
    jsonMode_ = (env != nullptr && env[0] != '\0');
}

SimulatorPlatform::~SimulatorPlatform() = default;

void SimulatorPlatform::registerDrivers(Runtime& rt) {
    runtime_ = &rt;

    // Init bridge first so it can subscribe before any events fire
    if (jsonMode_) {
        bridge_->init(rt);
    }

    // Init and register drivers
    battery_.init(rt.log(), rt.events());
    wifi_.init(rt.log(), rt.events(), &rt.asyncPoster());
    http_.init(rt.log(), &wifi_);   // gate HTTP on simulated WiFi link
    display_.init(rt.log(), *bridge_);

    rt.container().registerService(&battery_);
    rt.container().registerAs<IBatteryDriver>(&battery_);

    rt.container().registerService(&wifi_);
    rt.container().registerAs<IWifiDriver>(&wifi_);

    rt.container().registerService(&http_);
    rt.container().registerAs<IHttpClient>(&http_);

    rt.container().registerService(&display_);
    rt.container().registerAs<IDisplayDriver>(&display_);

    rt.container().registerService(&config_);
    rt.container().registerAs<IConfigStore>(&config_);

    // Init command reader
    cmdReader_->init(rt);

    // Register hardware & capabilities
    rt.hardware().add({"battery", DriverKind::Battery, "virtual 100%"});
    rt.hardware().add({"wifi",    DriverKind::Wifi,    "virtual"});
    rt.hardware().add({"display", DriverKind::Display, "virtual 264x176 1-bit"});

    rt.capabilities().add("battery");
    rt.capabilities().add("wifi");
    rt.capabilities().add("networking");
    rt.capabilities().add("http");
    rt.capabilities().add("display");
}

void SimulatorPlatform::idle() {
    if (jsonMode_) cmdReader_->poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

} // namespace kairo
