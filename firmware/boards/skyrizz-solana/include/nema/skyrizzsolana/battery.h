#pragma once
#include "nema/hal/battery.h"
#include "nema/service.h"
#include <cstdint>

namespace nema { class Runtime; class EventBus; }

namespace nema::skyrizzsolana {

// SolanaBattery — real battery gauge for SkyRizz Solana. Reads the pack voltage
// off the R18/R19 divider on GPIO1 (ADC1_CH0) via the ESP32-S3 one-shot ADC,
// maps it to a 0–100% level, and publishes BatteryChanged (like DummyBattery) so
// the status bar + Battery settings update. No charge-detect line on this board,
// so isCharging() is always false.
//
// Bring-up: DIVIDER and the 0%/100% voltage window are datasheet-guess defaults —
// verify against the real pack + measured divider ratio.
class SolanaBattery : public IBatteryDriver, public IService {
public:
    void init(Runtime& rt);

    // IDriver
    const char* name() const override { return "SolanaBattery"; }
    DriverKind  kind() const override { return DriverKind::Battery; }

    // IBatteryDriver
    int  level()      const override { return level_; }
    bool isCharging() const override { return false; }

    // IService
    void start() override;
    void stop()  override {}
    void tick(uint64_t nowMs) override;

private:
    bool readLevel(int& outPct);   // sample ADC → percent; false on read error
    void publish();

    Runtime*  rt_   = nullptr;
    EventBus* bus_  = nullptr;
    void*     adc_  = nullptr;      // adc_oneshot_unit_handle_t
    void*     cali_ = nullptr;      // adc_cali_handle_t (may be null → raw scaling)
    int       level_    = 100;
    uint64_t  lastMs_   = 0;
};

} // namespace nema::skyrizzsolana
