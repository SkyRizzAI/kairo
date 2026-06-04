#include "kairo/skyrizze32/lcd_driver.h"
#include "kairo/skyrizze32/xl9535.h"
#include "kairo/skyrizze32/board_config.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/service/service_container.h"
#include "kairo/config/config_store.h"
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <freertos/task.h>
#include <cstring>
#include <cstdio>

namespace kairo::skyrizze32 {

// MIPI DCS commands — common to ILI9341, ST7789, GC9A01, and most TFT panels.
static constexpr uint8_t CMD_SWRESET = 0x01;
static constexpr uint8_t CMD_SLPOUT  = 0x11;
static constexpr uint8_t CMD_DISPON  = 0x29;
static constexpr uint8_t CMD_CASET   = 0x2A;  // column address set
static constexpr uint8_t CMD_RASET   = 0x2B;  // row address set
static constexpr uint8_t CMD_RAMWR   = 0x2C;  // memory write
static constexpr uint8_t CMD_COLMOD  = 0x3A;
static constexpr uint8_t CMD_MADCTL  = 0x36;

// Multi-row RGB565 staging buffer (GUI-thread-only). Sending the frame in
// 32-row chunks instead of per-row cuts ~320 SPI transactions/frame down to
// ~10 — the per-transaction overhead, not the bitrate, was the bottleneck
// (~1 s/frame → ~0.2 s/frame). Sized for up to 320 px wide × 32 rows.
static constexpr int CHUNK_ROWS = 32;
static uint16_t chunkbuf_[320 * CHUNK_ROWS];

void LcdDriver::init(Runtime& rt, Xl9535& expander) {
    rt_       = &rt;
    expander_ = &expander;

    if (auto* cfg = rt.container().resolve<IConfigStore>()) {
        int w = (int)cfg->getIntOr("display", "width",  (int64_t)width_);
        int h = (int)cfg->getIntOr("display", "height", (int64_t)height_);
        if (w > 0 && w <= 1024) width_  = (uint16_t)w;
        if (h > 0 && h <= 1024) height_ = (uint16_t)h;
    }
}

void LcdDriver::start() {
    // 1-bit monochrome framebuffer
    size_t fbSize = ((size_t)width_ * height_ + 7) / 8;
    framebuf_ = (uint8_t*)heap_caps_malloc(fbSize, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!framebuf_) {
        if (rt_) rt_->log().error("LcdDriver", "framebuf alloc failed");
        return;
    }
    std::memset(framebuf_, 0x00, fbSize);  // start black (on=false = background)

    // SPI bus
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num    = PIN_LCD_MOSI;
    buscfg.miso_io_num    = -1;
    buscfg.sclk_io_num    = PIN_LCD_SCLK;
    buscfg.quadwp_io_num  = -1;
    buscfg.quadhd_io_num  = -1;
    buscfg.max_transfer_sz = 320 * 2 * CHUNK_ROWS;    // one 32-row chunk
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    // SPI device — 40 MHz. The Rust reference used 8 MHz "for cable tolerance",
    // but at 8 MHz a full 240×320×16bpp frame takes ~150 ms to shift out, which
    // makes touch-drag feel heavy. 40 MHz drops that to ~30 ms (~30 fps).
    // If the panel shows noise/artifacts on this flex, step down (26/20/8 MHz).
    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 40 * 1000 * 1000;
    devcfg.mode           = 0;
    devcfg.spics_io_num   = PIN_LCD_CS;
    devcfg.queue_size     = 1;
    spi_device_handle_t h;
    spi_bus_add_device(SPI2_HOST, &devcfg, &h);
    spiHandle_ = h;

    // DC pin
    gpio_set_direction((gpio_num_t)PIN_LCD_DC, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)PIN_LCD_DC, 0);

    panelInit();
    setBacklight(true);

    char ws[8], hs[8];
    snprintf(ws, sizeof(ws), "%d", (int)width_);
    snprintf(hs, sizeof(hs), "%d", (int)height_);
    if (rt_) rt_->log().info("LcdDriver", "started", {{"w", ws}, {"h", hs}});
}

void LcdDriver::stop() {
    setBacklight(false);
    if (framebuf_) { heap_caps_free(framebuf_); framebuf_ = nullptr; }
}

// ── Panel init ────────────────────────────────────────────────────────────
// ILI9341, 240×320 portrait. Adafruit-style sequence ported verbatim from the
// working SkyRizz E32 Rust reference firmware (power/VCOM/gamma all required —
// a bare SLPOUT+DISPON leaves this panel blank).
void LcdDriver::panelInit() {
    auto cmd = [&](uint8_t c) {
        spiWrite(&c, 1, false);
    };
    auto data = [&](std::initializer_list<uint8_t> bytes) {
        uint8_t buf[16];
        size_t  n = 0;
        for (uint8_t b : bytes) buf[n++] = b;
        spiWrite(buf, n, true);
    };

    cmd(CMD_SWRESET);             vTaskDelay(pdMS_TO_TICKS(150));
    cmd(CMD_SLPOUT);              vTaskDelay(pdMS_TO_TICKS(150));

    // Power / timing
    cmd(0xCB); data({0x39, 0x2C, 0x00, 0x34, 0x02});
    cmd(0xCF); data({0x00, 0xC1, 0x30});
    cmd(0xE8); data({0x85, 0x00, 0x78});
    cmd(0xEA); data({0x00, 0x00});
    cmd(0xED); data({0x64, 0x03, 0x12, 0x81});
    cmd(0xF7); data({0x20});
    cmd(0xC0); data({0x23});             // Power control 1
    cmd(0xC1); data({0x10});             // Power control 2
    cmd(0xC5); data({0x3E, 0x28});       // VCOM control 1
    cmd(0xC7); data({0x86});             // VCOM control 2

    cmd(CMD_MADCTL); data({0x48});       // MX | BGR → portrait 240×320
    cmd(CMD_COLMOD); data({0x55});       // 16-bit RGB565

    cmd(0xB1); data({0x00, 0x1B});       // frame rate ~70 Hz
    cmd(0xB6); data({0x08, 0x82, 0x27});

    // Gamma
    cmd(0xF2); data({0x00});
    cmd(0x26); data({0x01});
    cmd(0xE0); data({0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                     0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00});
    cmd(0xE1); data({0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                     0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F});

    cmd(CMD_DISPON);              vTaskDelay(pdMS_TO_TICKS(100));
}

// ── Address window helper ─────────────────────────────────────────────────
void LcdDriver::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t buf[4];

    buf[0] = x0 >> 8; buf[1] = x0 & 0xFF;
    buf[2] = x1 >> 8; buf[3] = x1 & 0xFF;
    uint8_t c = CMD_CASET;
    spiWrite(&c, 1, false);
    spiWrite(buf, 4, true);

    buf[0] = y0 >> 8; buf[1] = y0 & 0xFF;
    buf[2] = y1 >> 8; buf[3] = y1 & 0xFF;
    c = CMD_RASET;
    spiWrite(&c, 1, false);
    spiWrite(buf, 4, true);

    c = CMD_RAMWR;
    spiWrite(&c, 1, false);
}

// ── Draw primitives ───────────────────────────────────────────────────────
void LcdDriver::clear(bool on) {
    if (!framebuf_) return;
    size_t sz = ((size_t)width_ * height_ + 7) / 8;
    std::memset(framebuf_, on ? 0xFF : 0x00, sz);
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

void LcdDriver::invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!framebuf_) return;
    for (uint16_t ry = y; ry < y + h && ry < height_; ry++)
        for (uint16_t rx = x; rx < x + w && rx < width_; rx++) {
            size_t idx = (size_t)ry * width_ + rx;
            framebuf_[idx / 8] ^= (uint8_t)(0x80 >> (idx % 8));
        }
}

// ── Flush: expand 1-bit framebuf → RGB565, send in 32-row chunks ─────────
void LcdDriver::flush() {
    if (!framebuf_ || !spiHandle_) return;

    setWindow(0, 0, (uint16_t)(width_ - 1), (uint16_t)(height_ - 1));

    for (uint16_t y0 = 0; y0 < height_; y0 += CHUNK_ROWS) {
        uint16_t rows = (uint16_t)((y0 + CHUNK_ROWS <= height_)
                                       ? CHUNK_ROWS : (height_ - y0));
        size_t n = 0;
        for (uint16_t y = y0; y < y0 + rows; y++) {
            for (uint16_t x = 0; x < width_; x++) {
                size_t idx = (size_t)y * width_ + x;
                bool   on  = (framebuf_[idx / 8] >> (7 - (idx % 8))) & 1;
                // Panel is hardware-inverted: invert colors before sending so
                // fgColor_/bgColor_ are the actual on-screen colors.
                chunkbuf_[n++] = on ? (uint16_t)~fgColor_ : (uint16_t)~bgColor_;
            }
        }
        spiWrite((uint8_t*)chunkbuf_, n * 2, true);   // one transaction per chunk
    }
}

// ── Direct RGB565 blit (camera viewfinder) ───────────────────────────────
// GC2145 outputs standard RGB565. ILI9341 is in BGR mode (MADCTL=0x48, D3=1),
// so R and B must be swapped. Panel is also hardware-inverted, so invert (~).
void LcdDriver::blitRgb565(const uint8_t* buf,
                             uint16_t x, uint16_t y,
                             uint16_t w, uint16_t h) {
    if (!spiHandle_ || !buf || w == 0 || h == 0) return;
    setWindow(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));

    // Batch CHUNK_ROWS rows per SPI transaction to cut transaction overhead
    // (240 transactions → ~8 transactions for a 240×240 frame).
    for (uint16_t row = 0; row < h; ) {
        uint16_t batch  = (uint16_t)((h - row) < (uint16_t)CHUNK_ROWS
                                     ? (h - row) : (uint16_t)CHUNK_ROWS);
        size_t   pixels = (size_t)batch * w;

        for (size_t i = 0; i < pixels; i++) {
            size_t   idx = (((size_t)row * w) + i) * 2;
            uint16_t rgb = ((uint16_t)buf[idx] << 8) | buf[idx + 1];
            // Swap R↔B: GC2145 RGB → ILI9341 BGR
            uint8_t  r   = (rgb >> 11) & 0x1F;
            uint8_t  g   = (rgb >>  5) & 0x3F;
            uint8_t  b   =  rgb         & 0x1F;
            uint16_t bgr = ((uint16_t)b << 11) | ((uint16_t)g << 5) | r;
            // Invert for panel hardware inversion
            chunkbuf_[i] = (uint16_t)~bgr;
        }
        spiWrite((uint8_t*)chunkbuf_, pixels * 2, true);
        row = (uint16_t)(row + batch);
    }
}

// ── Backlight + SPI ───────────────────────────────────────────────────────
void LcdDriver::setBacklight(bool on) {
    if (expander_) expander_->setBacklight(on);
}

void LcdDriver::spiWrite(uint8_t* data, size_t len, bool isData) {
    if (!len) return;
    gpio_set_level((gpio_num_t)PIN_LCD_DC, isData ? 1 : 0);
    spi_transaction_t t = {};
    t.length    = len * 8;
    t.tx_buffer = data;
    spi_device_transmit((spi_device_handle_t)spiHandle_, &t);
}

} // namespace kairo::skyrizze32
