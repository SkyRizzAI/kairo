#include "kairo/skyrizze32/es7243e_mic.h"
#include "kairo/skyrizze32/board_config.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include <driver/i2s_std.h>
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
    // Full-duplex I2S0: TX → NS4168 (GPIO45), RX ← ES7243E (GPIO39).
    // Per IDF 5.x full-duplex pattern: call i2s_channel_init_std_mode on BOTH
    // handles with the SAME config (dout + din both set). TX handle configures
    // the clocks; RX handle re-uses the same config so GPIOs register properly.
    i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chanCfg.auto_clear = true;  // TX outputs silence when no data written
    i2s_chan_handle_t tx = nullptr;
    i2s_chan_handle_t rx = nullptr;
    i2s_new_channel(&chanCfg, &tx, &rx);

    // MCLK = 256 * 16 kHz = 4.096 MHz (ES7243E slaves on this)
    i2s_std_clk_config_t clkCfg = I2S_STD_CLK_DEFAULT_CONFIG(16000);
    clkCfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    // Single config with ALL pins — both TX and RX handles get the same struct.
    // IDF routes .dout when called on TX handle, .din when called on RX handle.
    i2s_std_config_t cfg = {
        .clk_cfg  = clkCfg,
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                         I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = (gpio_num_t)PIN_I2S_MCLK,  // GPIO3  → ES7243E
            .bclk = (gpio_num_t)PIN_I2S_BCLK,  // GPIO0  → ES7243E + NS4168
            .ws   = (gpio_num_t)PIN_I2S_WS,    // GPIO38 → ES7243E + NS4168
            .dout = (gpio_num_t)PIN_I2S_DOUT,  // GPIO45 → NS4168 SDI
            .din  = (gpio_num_t)PIN_I2S_DIN,   // GPIO39 ← ES7243E SDO
            .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
        },
    };
    i2s_channel_init_std_mode(tx, &cfg);
    i2s_channel_init_std_mode(rx, &cfg);

    i2sRxHandle_ = rx;
    i2sTxHandle_ = tx;
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
    // Step 1: Start I2S first — ES7243E needs MCLK before I2C will ACK
    i2sInit();

    if (!i2sRxHandle_) {
        if (rt_) rt_->log().error("Es7243eMic", "I2S init failed");
        return;
    }

    // Enable channels so MCLK is live
    i2s_channel_enable((i2s_chan_handle_t)i2sRxHandle_);
    i2s_channel_enable((i2s_chan_handle_t)i2sTxHandle_);

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
    if (i2sRxHandle_) {
        i2s_channel_disable((i2s_chan_handle_t)i2sRxHandle_);
        i2s_del_channel((i2s_chan_handle_t)i2sRxHandle_);
        i2sRxHandle_ = nullptr;
    }
    if (i2sTxHandle_) {
        i2s_channel_disable((i2s_chan_handle_t)i2sTxHandle_);
        i2s_del_channel((i2s_chan_handle_t)i2sTxHandle_);
        i2sTxHandle_ = nullptr;
    }
    capturing_ = false;
    peak_       = 0.0f;
}

void Es7243eMic::startCapture() {
    if (i2sRxHandle_ && !capturing_) {
        i2s_channel_enable((i2s_chan_handle_t)i2sRxHandle_);
        capturing_ = true;
    }
}

void Es7243eMic::stopCapture() {
    if (i2sRxHandle_ && capturing_) {
        i2s_channel_disable((i2s_chan_handle_t)i2sRxHandle_);
        capturing_ = false;
    }
}

void Es7243eMic::tick(uint64_t /*nowMs*/) {
    if (!i2sRxHandle_ || !capturing_) return;

    static int32_t buf[512];
    size_t bytesRead = 0;
    esp_err_t err = i2s_channel_read((i2s_chan_handle_t)i2sRxHandle_,
                                      buf, sizeof(buf), &bytesRead, pdMS_TO_TICKS(10));
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
