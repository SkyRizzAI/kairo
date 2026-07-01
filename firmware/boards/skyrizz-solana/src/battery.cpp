#include "nema/skyrizzsolana/battery.h"
#include "nema/skyrizzsolana/board_config.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/event/event.h"
#include "nema/event/event_bus.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <string>

namespace nema::skyrizzsolana {

// GPIO1 = ADC1 channel 0 on the ESP32-S3.
static constexpr adc_channel_t kChan = ADC_CHANNEL_0;
// R18/R19 resistor divider on ADCBAT (guess: 2:1). pack_mV = pin_mV * DIVIDER.
static constexpr float DIVIDER   = 2.0f;
// Li-ion/LiPo working window mapped to 0..100%.
static constexpr int   MV_EMPTY  = 3300;
static constexpr int   MV_FULL   = 4200;
static constexpr uint32_t SAMPLE_MS = 30000;   // re-sample every 30 s

void SolanaBattery::init(Runtime& rt) {
    rt_  = &rt;
    bus_ = &rt.events();
}

void SolanaBattery::start() {
    adc_oneshot_unit_init_cfg_t ucfg = {};
    ucfg.unit_id = ADC_UNIT_1;
    adc_oneshot_unit_handle_t unit = nullptr;
    if (adc_oneshot_new_unit(&ucfg, &unit) != ESP_OK) {
        if (rt_) rt_->log().warn("SolanaBattery", "adc unit init failed");
        return;
    }
    adc_oneshot_chan_cfg_t ccfg = {};
    ccfg.atten    = ADC_ATTEN_DB_12;   // ~0..3.1 V range (pack/2 fits)
    ccfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_oneshot_config_channel(unit, kChan, &ccfg);
    adc_ = unit;

    // Optional curve-fitting calibration → accurate mV; fall back to raw scaling.
    adc_cali_curve_fitting_config_t cc = {};
    cc.unit_id  = ADC_UNIT_1;
    cc.atten    = ADC_ATTEN_DB_12;
    cc.bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_cali_handle_t cali = nullptr;
    if (adc_cali_create_scheme_curve_fitting(&cc, &cali) == ESP_OK) cali_ = cali;

    int pct;
    if (readLevel(pct)) level_ = pct;
    publish();
    if (rt_) rt_->log().info("SolanaBattery", "started", {{"pct", std::to_string(level_)}});
}

bool SolanaBattery::readLevel(int& outPct) {
    if (!adc_) return false;
    int raw = 0;
    if (adc_oneshot_read((adc_oneshot_unit_handle_t)adc_, kChan, &raw) != ESP_OK) return false;

    int pinMv;
    if (cali_) {
        if (adc_cali_raw_to_voltage((adc_cali_handle_t)cali_, raw, &pinMv) != ESP_OK) return false;
    } else {
        pinMv = raw * 3100 / 4095;   // rough: 12-bit @ 12dB ≈ 3.1 V full scale
    }
    int packMv = (int)(pinMv * DIVIDER);
    int pct = (packMv - MV_EMPTY) * 100 / (MV_FULL - MV_EMPTY);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    outPct = pct;
    return true;
}

void SolanaBattery::publish() {
    if (!bus_) return;
    bus_->publish({events::BatteryChanged, {
        {"level", std::to_string(level_)},
        {"charging", "0"}}});
}

void SolanaBattery::tick(uint64_t nowMs) {
    if (nowMs - lastMs_ < SAMPLE_MS) return;
    lastMs_ = nowMs;
    int pct;
    if (readLevel(pct) && pct != level_) { level_ = pct; publish(); }
}

} // namespace nema::skyrizzsolana
