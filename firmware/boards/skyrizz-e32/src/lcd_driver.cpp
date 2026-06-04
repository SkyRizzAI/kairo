#include "kairo/skyrizze32/lcd_driver.h"
#include "kairo/skyrizze32/xl9535.h"
#include "kairo/skyrizze32/board_config.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/config/config_store.h"
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <cstring>

namespace kairo::skyrizze32 {

// Panel controller commands (ST7789 / ILI9341 compatible subset).
// Adjust panelInit() if the fitted panel differs.
static constexpr uint8_t CMD_SWRESET = 0x01;
static constexpr uint8_t CMD_SLPOUT  = 0x11;
static constexpr uint8_t CMD_DISPON  = 0x29;
static constexpr uint8_t CMD_COLMOD  = 0x3A;
static constexpr uint8_t CMD_MADCTL  = 0x36;

void LcdDriver::init(Runtime& rt, Xl9535& expander) {
    rt_       = &rt;
    expander_ = &expander;

    // Load resolution from config if available (set after panel probe at first boot)
    if (auto* cfg = rt.container().resolve<IConfigStore>()) {
        int w = (int)cfg->getIntOr("display", "width",  (int64_t)width_);
        int h = (int)cfg->getIntOr("display", "height", (int64_t)height_);
        if (w > 0 && w <= 1024) width_  = (uint16_t)w;
        if (h > 0 && h <= 1024) height_ = (uint16_t)h;
    }
}

void LcdDriver::start() {
    // Allocate 1-bit framebuffer in DMA-capable SRAM
    size_t fbSize = ((size_t)width_ * height_ + 7) / 8;
    framebuf_ = (uint8_t*)heap_caps_malloc(fbSize, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!framebuf_) {
        if (rt_) rt_->log().error("LcdDriver", "framebuf alloc failed");
        return;
    }
    std::memset(framebuf_, 0, fbSize);

    // SPI bus + device
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = PIN_LCD_MOSI;
    buscfg.miso_io_num = -1;
    buscfg.sclk_io_num = PIN_LCD_SCLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = (int)fbSize;
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 40 * 1000 * 1000;   // 40 MHz
    devcfg.mode           = 0;
    devcfg.spics_io_num   = PIN_LCD_CS;
    devcfg.queue_size     = 7;
    spi_device_handle_t h;
    spi_bus_add_device(SPI2_HOST, &devcfg, &h);
    spiHandle_ = h;

    // DC pin
    gpio_set_direction((gpio_num_t)PIN_LCD_DC, GPIO_MODE_OUTPUT);

    panelInit();
    setBacklight(true);
    if (rt_) rt_->log().info("LcdDriver", "started",
        {{"w", std::to_string(width_)}, {"h", std::to_string(height_)}});
}

void LcdDriver::stop() {
    setBacklight(false);
    if (framebuf_) { heap_caps_free(framebuf_); framebuf_ = nullptr; }
}

void LcdDriver::panelInit() {
    // Minimal ST7789-compatible init. Swap for actual panel at bring-up.
    auto cmd = [&](uint8_t c, const uint8_t* d = nullptr, size_t len = 0) {
        spiWrite(&c, 1, false);
        if (d && len) spiWrite(const_cast<uint8_t*>(d), len, true);
    };
    cmd(CMD_SWRESET); vTaskDelay(pdMS_TO_TICKS(150));
    cmd(CMD_SLPOUT);  vTaskDelay(pdMS_TO_TICKS(10));
    uint8_t colmod = 0x55;   // 16-bit color (we blit 1-bit expanded to RGB565)
    cmd(CMD_COLMOD, &colmod, 1);
    uint8_t madctl = 0x00;
    cmd(CMD_MADCTL, &madctl, 1);
    cmd(CMD_DISPON);
}

void LcdDriver::drawPixel(uint16_t x, uint16_t y, bool on) {
    if (!framebuf_ || x >= width_ || y >= height_) return;
    size_t idx = (size_t)y * width_ + x;
    if (on) framebuf_[idx / 8] |=  (uint8_t)(0x80 >> (idx % 8));
    else    framebuf_[idx / 8] &= ~(uint8_t)(0x80 >> (idx % 8));
}

void LcdDriver::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) {
    for (uint16_t ry = y; ry < y + h && ry < height_; ry++)
        for (uint16_t rx = x; rx < x + w && rx < width_; rx++)
            drawPixel(rx, ry, on);
}

void LcdDriver::flush() {
    if (!framebuf_ || !spiHandle_) return;
    // Expand 1-bit framebuffer → RGB565 and DMA to panel.
    // For brevity this uses a direct byte-at-a-time approach;
    // optimise to DMA batch transfer at bring-up.
    size_t pixels = (size_t)width_ * height_;
    for (size_t i = 0; i < pixels; i++) {
        bool on  = (framebuf_[i / 8] >> (7 - (i % 8))) & 1;
        uint8_t pix[2] = { on ? (uint8_t)0xFF : (uint8_t)0x00,
                           on ? (uint8_t)0xFF : (uint8_t)0x00 };
        spiWrite(pix, 2, true);
    }
}

void LcdDriver::setBacklight(bool on) {
    if (expander_) expander_->setBacklight(on);
}

void LcdDriver::spiWrite(uint8_t* data, size_t len, bool isData) {
    gpio_set_level((gpio_num_t)PIN_LCD_DC, isData ? 1 : 0);
    spi_transaction_t t = {};
    t.length    = len * 8;
    t.tx_buffer = data;
    spi_device_transmit((spi_device_handle_t)spiHandle_, &t);
}

} // namespace kairo::skyrizze32
