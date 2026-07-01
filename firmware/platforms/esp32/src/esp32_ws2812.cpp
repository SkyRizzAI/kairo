#include "nema/esp32/esp32_ws2812.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"   // esp_rom_delay_us

namespace nema {

// RMT resolution 10 MHz → 1 tick = 0.1 µs. WS2812 timings (T0H .35/T0L .8,
// T1H .7/T1L .6 µs, ±150ns) as symbol durations in ticks.
static constexpr uint32_t kResHz = 10 * 1000 * 1000;

Esp32Ws2812::Esp32Ws2812(int gpio, int count) : gpio_(gpio), count_(count) {
    if (count_ < 1) count_ = 1;
    buf_.assign((size_t)count_ * 3, 0);
}

bool Esp32Ws2812::begin() {
    if (ready_) return true;

    rmt_tx_channel_config_t txcfg = {};
    txcfg.gpio_num          = (gpio_num_t)gpio_;
    txcfg.clk_src           = RMT_CLK_SRC_DEFAULT;
    txcfg.resolution_hz     = kResHz;
    txcfg.mem_block_symbols = 64;
    txcfg.trans_queue_depth = 4;
    rmt_channel_handle_t chan = nullptr;
    if (rmt_new_tx_channel(&txcfg, &chan) != ESP_OK) return false;

    rmt_bytes_encoder_config_t bcfg = {};
    bcfg.bit0.level0 = 1; bcfg.bit0.duration0 = 3;   // 0.3 µs high
    bcfg.bit0.level1 = 0; bcfg.bit0.duration1 = 9;   // 0.9 µs low
    bcfg.bit1.level0 = 1; bcfg.bit1.duration0 = 9;   // 0.9 µs high
    bcfg.bit1.level1 = 0; bcfg.bit1.duration1 = 3;   // 0.3 µs low
    bcfg.flags.msb_first = 1;
    rmt_encoder_handle_t enc = nullptr;
    if (rmt_new_bytes_encoder(&bcfg, &enc) != ESP_OK) {
        rmt_del_channel(chan);
        return false;
    }
    if (rmt_enable(chan) != ESP_OK) {
        rmt_del_encoder(enc);
        rmt_del_channel(chan);
        return false;
    }

    chan_  = chan;
    enc_   = enc;
    ready_ = true;
    clear();
    show();
    return true;
}

void Esp32Ws2812::setPixel(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 0 || index >= count_) return;
    // Apply global brightness, store GRB (WS2812 wire order).
    uint8_t rr = (uint8_t)((uint16_t)r * bright_ / 255);
    uint8_t gg = (uint8_t)((uint16_t)g * bright_ / 255);
    uint8_t bb = (uint8_t)((uint16_t)b * bright_ / 255);
    size_t o = (size_t)index * 3;
    buf_[o + 0] = gg;
    buf_[o + 1] = rr;
    buf_[o + 2] = bb;
}

void Esp32Ws2812::setAll(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < count_; i++) setPixel(i, r, g, b);
}

void Esp32Ws2812::clear() {
    for (auto& v : buf_) v = 0;
}

void Esp32Ws2812::show() {
    if (!ready_) return;
    rmt_transmit_config_t tx = {};
    tx.loop_count = 0;
    rmt_transmit((rmt_channel_handle_t)chan_, (rmt_encoder_handle_t)enc_,
                 buf_.data(), buf_.size(), &tx);
    rmt_tx_wait_all_done((rmt_channel_handle_t)chan_, portMAX_DELAY);
    // WS2812 latch: hold the line low >50 µs before the next frame.
    esp_rom_delay_us(60);
}

} // namespace nema
