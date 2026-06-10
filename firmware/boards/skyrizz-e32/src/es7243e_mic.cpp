#include "kairo/skyrizze32/es7243e_mic.h"
#include "kairo/skyrizze32/board_config.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include <driver/i2s.h>   // legacy I2S API — matches proven factory HW test
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>

namespace kairo::skyrizze32 {

// ES7243E init sequence — verbatim from Rust reference main.rs es7243_init().
// IMPORTANT: ES7243E requires MCLK to be running before any I2C write will ACK.
// Therefore i2sInit() (starts MCLK) must be called BEFORE i2cInit().
static const uint8_t kEs7243eInit[][2] = {
    {0x01, 0x3A}, {0x00, 0x80}, {0xF9, 0x00},
    {0x04, 0x02}, {0x04, 0x01}, {0xF9, 0x01}, {0x00, 0x1E},
    {0x01, 0x00}, {0x02, 0x00}, {0x03, 0x20}, {0x04, 0x01},
    {0x0D, 0x00}, {0x05, 0x00}, {0x06, 0x03}, {0x07, 0x00},
    {0x08, 0xFF}, {0x09, 0xCA},
    {0x0A, 0x85}, {0x0B, 0x00},
    {0x0E, 0xBF}, {0x0F, 0x80},
    {0x14, 0x0C}, {0x15, 0x0C},
    {0x17, 0x02}, {0x18, 0x26}, {0x19, 0x77}, {0x1A, 0xF4},
    {0x1B, 0x66}, {0x1C, 0x44}, {0x1E, 0x00}, {0x1F, 0x0C},
    // PGA gain: 24 dB → 0x18
    {0x20, 0x18}, {0x21, 0x18},
    {0x00, 0x80}, {0x01, 0x3A},
    {0x16, 0x3F}, {0x16, 0x00},
    {0x20, 0x18}, {0x21, 0x18},
    {0x00, 0x80}, {0x01, 0x3A},
    {0x16, 0x3F}, {0x16, 0x00},
};
static constexpr size_t kEs7243eInitLen = sizeof(kEs7243eInit) / sizeof(kEs7243eInit[0]);

void Es7243eMic::init(kairo::Runtime& rt, Xl9535& expander) {
    rt_       = &rt;
    expander_ = &expander;
}

void Es7243eMic::i2sInit() {
    // Legacy I2S driver (driver/i2s.h) — single full-duplex install on I2S0:
    //   TX → NS4168 SDI (GPIO45),  RX ← ES7243E SDO (GPIO39).
    // Verbatim match of the proven factory hardware test
    // (ESP32S3_Full_HW_Test_Lovyan_V4_1_GTSD_Fix.ino, audioI2SInit()).
    // The new i2s_std full-duplex driver's writes succeed (err=0, full bytes)
    // but produce NO audible TX output on this board; the legacy driver drives
    // the shared bus correctly. The mic (RX) works under both APIs.
    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
    cfg.sample_rate          = 16000;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_desc_num         = 6;    // legacy alias: dma_buf_count
    cfg.dma_frame_num        = 128;  // legacy alias: dma_buf_len
    cfg.use_apll             = false;
    cfg.tx_desc_auto_clear   = true;
    cfg.fixed_mclk           = 16000 * 256;  // 4.096 MHz MCLK → ES7243E

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
    if (err != ESP_OK) {
        if (rt_) rt_->log().error("Es7243eMic", "i2s_driver_install failed",
            {{"err", esp_err_to_name(err)}});
        return;
    }

    i2s_pin_config_t pins = {};
    pins.mck_io_num   = PIN_I2S_MCLK;   // GPIO3  → ES7243E MCLK
    pins.bck_io_num   = PIN_I2S_BCLK;   // GPIO0  → ES7243E + NS4168
    pins.ws_io_num    = PIN_I2S_WS;     // GPIO38 → ES7243E + NS4168
    pins.data_out_num = PIN_I2S_DOUT;   // GPIO45 → NS4168 SDI
    pins.data_in_num  = PIN_I2S_DIN;    // GPIO39 ← ES7243E SDO
    i2s_set_pin(I2S_NUM_0, &pins);

    i2s_zero_dma_buffer(I2S_NUM_0);
    i2sInstalled_ = true;
}

bool Es7243eMic::i2cInit() {
    bool ok = true;
    for (size_t i = 0; i < kEs7243eInitLen; i++) {
        Wire.beginTransmission(I2C_ADDR_ES7243E);
        Wire.write(kEs7243eInit[i][0]);
        Wire.write(kEs7243eInit[i][1]);
        if (Wire.endTransmission() != 0) ok = false;
    }
    return ok;
}

void Es7243eMic::start() {
    // Step 1: Install I2S first — ES7243E needs MCLK before I2C will ACK.
    // i2s_driver_install already starts the port with clocks live.
    i2sInit();

    if (!i2sInstalled_) {
        if (rt_) rt_->log().error("Es7243eMic", "I2S init failed");
        return;
    }

    // Step 2: Wait for ES7243E to see stable MCLK, then init via I2C
    vTaskDelay(pdMS_TO_TICKS(10));

    Wire.beginTransmission(I2C_ADDR_ES7243E);
    bool ack = (Wire.endTransmission() == 0);
    if (!ack) {
        if (rt_) rt_->log().warn("Es7243eMic", "ES7243E not found at 0x11 (no MCLK?)");
    } else {
        if (!i2cInit())
            if (rt_) rt_->log().warn("Es7243eMic", "ES7243E I2C init incomplete");
    }

    capturing_ = true;
    if (rt_) rt_->log().info("Es7243eMic", "started",
        {{"mclk", std::to_string(PIN_I2S_MCLK)},
         {"din",  std::to_string(PIN_I2S_DIN)},
         {"dout", std::to_string(PIN_I2S_DOUT)}});
}

void Es7243eMic::stop() {
    if (i2sInstalled_) {
        i2s_driver_uninstall(I2S_NUM_0);
        i2sInstalled_ = false;
    }
    capturing_ = false;
    peak_       = 0.0f;
}

// Capture toggles a software flag only — the legacy driver keeps the shared
// port running so MCLK stays live for both ES7243E and the NS4168 speaker.
// (i2s_stop() would kill the clock for the whole port.)
void Es7243eMic::startCapture() {
    if (i2sInstalled_) capturing_ = true;
}

void Es7243eMic::stopCapture() {
    capturing_ = false;
}

void Es7243eMic::tick(uint64_t /*nowMs*/) {
    if (!i2sInstalled_ || !capturing_) return;

    static int32_t buf[512];
    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_NUM_0, buf, sizeof(buf), &bytesRead, pdMS_TO_TICKS(10));
    if (err != ESP_OK || bytesRead == 0) return;

    size_t samples = bytesRead / sizeof(int32_t);
    int64_t peak = 0;
    for (size_t i = 0; i < samples; i++) {
        // ES7243E outputs 24-bit audio MSB-justified in 32-bit I2S slots.
        // Right-shift 8 to get 24-bit signed value, then take absolute value.
        int64_t v = (int64_t)(buf[i] >> 8);
        if (v < 0) v = -v;
        if (v > peak) peak = v;
    }
    // Normalize against 24-bit full scale (0x7FFFFF = 8388607).
    // Apply 4× gain boost so typical speech (~6 dBFS) reads near 50-80%.
    const float gain = 4.0f;
    peak_ = gain * (float)peak / (float)0x7FFFFF;
    if (peak_ > 1.0f) peak_ = 1.0f;
}

} // namespace kairo::skyrizze32
