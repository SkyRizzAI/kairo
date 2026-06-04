#pragma once
#include "kairo/platform.h"
#include "kairo/sim/host_clock.h"
#include "kairo/sim/sim_battery_driver.h"
#include "kairo/sim/sim_wifi_driver.h"
#include "kairo/sim/sim_http_client.h"
#include "kairo/sim/sim_display.h"
#include "kairo/sim/mem_config_store.h"

namespace kairo {

class CommandReader;
class TelemetryBridge;

class SimulatorPlatform : public IPlatform {
public:
    SimulatorPlatform();
    ~SimulatorPlatform();

    const char* name() const override { return "simulator"; }
    IClock& clock() override { return clock_; }
    OutputMode outputMode() const override {
        return jsonMode_ ? OutputMode::Json : OutputMode::Human;
    }
    void registerDrivers(Runtime& rt) override;
    void idle() override;

    bool isJsonMode() const { return jsonMode_; }

    // Expose drivers so board/target can reference them
    SimBatteryDriver& battery() { return battery_; }
    SimWifiDriver&    wifi()    { return wifi_; }

private:
    HostClock          clock_;
    SimBatteryDriver   battery_;
    SimWifiDriver      wifi_;
    SimHttpClient      http_;
    SimDisplay         display_;
    MemConfigStore     config_;
    bool               jsonMode_ = false;
    Runtime*           runtime_  = nullptr;

    std::unique_ptr<CommandReader>    cmdReader_;
    std::unique_ptr<TelemetryBridge>  bridge_;
};

} // namespace kairo
