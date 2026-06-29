#include "nema/skyrizzsolana/lcd_driver.h"
#include "nema/skyrizzsolana/board_config.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/service/service_container.h"
#include "nema/config/config_store.h"
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <freertos/task.h>
#include <cstring>
#include <cstdio>

namespace nema::skyrizzsolana {

// MIPI DCS commands — common to ILI9341, ST7789, GC9A01, and most TFT panels.
static constexpr uint8_t CMD_SWRESET = 0x01;
static constexpr uint8_t CMD_SLPOUT  = 0x11;
static constexpr uint8_t CMD_DISPON  = 0x29;
static constexpr uint8_t CMD_CASET   = 0x2A;
static constexpr uint8_t CMD_RASET   = 0x2B;
static constexpr uint8_t CMD_RAMWR   = 0x2C;
static constexpr uint8_t CMD_COLMOD  = 0x3A;
static constexpr uint8_t CMD_MADCTL  = 0x36;

// Multi-row RGB565 staging buffer (GUI-thread-only). 8 rows/chunk keeps internal
// SRAM free for the BLE controller (Plan 93). Sized for up to 320 px × 8 rows.
static constexpr int CHUNK_ROWS = 8;
static uint16_t chunkbuf_[320 * CHUNK_ROWS];

void LcdDriver::init(Runtime& rt) {
    rt_ = &rt;

    if (auto* cfg = rt.container().resolve<IConfigStore>()) {
        int w = (int)cfg->getIntOr("display", "width",  (int64_t)width_);
        int h = (int)cfg->getIntOr("display", "height", (int64_t)height_);
        if (w > 0 && w <= 1024) width_  = (uint16_t)w;
        if (h > 0 && h <= 1024) height_ = (uint16_t)h;
        nativeW_ = width_; nativeH_ = height_;   // portrait native (pre-rotation)

        rotation_ = (uint8_t)(cfg->getIntOr("display", "rotation", 0) & 3);
        if (rotation_ == 1 || rotation_ == 3) {
            uint16_t t = width_; width_ = height_; height_ = t;
        }
        brightness_ = (uint8_t)cfg->getIntOr("display", "brightness", 255);
    }
}

// Backlight has no PWM on this board (plain GPIO7) — map level >0 → on, 0 → off.
void LcdDriver::setBrightness(uint8_t level) {
    brightness_ = level;
    setBacklight(level > 0);
}

void LcdDriver::start() {
    // 1-bit monochrome framebuffer in PSRAM (CPU-read into chunkbuf_ for SPI DMA,
    // never DMA'd itself) — frees internal SRAM for the BLE controller (Plan 93).
    size_t fbSize = ((size_t)width_ * height_ + 7) / 8;
    framebuf_ = (uint8_t*)heap_caps_malloc(fbSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!framebuf_) framebuf_ = (uint8_t*)heap_caps_malloc(fbSize, MALLOC_CAP_8BIT);
    if (!framebuf_) {
        if (rt_) rt_->log().error("LcdDriver", "framebuf alloc failed");
        return;
    }
    std::memset(framebuf_, 0x00, fbSize);

    prevBuf_ = (uint8_t*)heap_caps_malloc(fbSize, MALLOC_CAP_SPIRAM);
    if (!prevBuf_) prevBuf_ = (uint8_t*)heap_caps_malloc(fbSize, MALLOC_CAP_8BIT);
    fullFlush_ = true;

    // SPI bus
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num     = PIN_LCD_MOSI;
    buscfg.miso_io_num     = -1;
    buscfg.sclk_io_num     = PIN_LCD_SCLK;
    buscfg.quadwp_io_num   = -1;
    buscfg.quadhd_io_num   = -1;
    buscfg.max_transfer_sz = 320 * 2 * CHUNK_ROWS;
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 40 * 1000 * 1000;   // step down (26/20/8 MHz) if noisy
    devcfg.mode           = 0;
    devcfg.spics_io_num   = PIN_LCD_CS;
    devcfg.queue_size     = 1;
    spi_device_handle_t h;
    spi_bus_add_device(SPI2_HOST, &devcfg, &h);
    spiHandle_ = h;

    // DC pin
    gpio_set_direction((gpio_num_t)PIN_LCD_DC, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)PIN_LCD_DC, 0);

    // Backlight pin (start off until the panel is up to avoid a flash of noise).
    gpio_set_direction((gpio_num_t)PIN_LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)PIN_LCD_BL, 0);

    // Hardware reset pulse on the dedicated LCDRST line (GPIO14, active-LOW).
    gpio_set_direction((gpio_num_t)PIN_LCD_RST, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)PIN_LCD_RST, 1);   vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)PIN_LCD_RST, 0);   vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level((gpio_num_t)PIN_LCD_RST, 1);   vTaskDelay(pdMS_TO_TICKS(120));

    panelInit();
    setBacklight(brightness_ > 0);

    char ws[8], hs[8];
    snprintf(ws, sizeof(ws), "%d", (int)width_);
    snprintf(hs, sizeof(hs), "%d", (int)height_);
    if (rt_) rt_->log().info("LcdDriver", "started", {{"w", ws}, {"h", hs}});
}

void LcdDriver::stop() {
    setBacklight(false);
    if (framebuf_) { heap_caps_free(framebuf_); framebuf_ = nullptr; }
    if (prevBuf_)  { heap_caps_free(prevBuf_);  prevBuf_  = nullptr; }
}

// ── Panel init ────────────────────────────────────────────────────────────
// ILI9341, 240×320 portrait. Adafruit-style sequence (power/VCOM/gamma all
// required — a bare SLPOUT+DISPON leaves this panel blank).
void LcdDriver::panelInit() {
    auto cmd = [&](uint8_t c) { spiWrite(&c, 1, false); };
    auto data = [&](std::initializer_list<uint8_t> bytes) {
        uint8_t buf[16]; size_t n = 0;
        for (uint8_t b : bytes) buf[n++] = b;
        spiWrite(buf, n, true);
    };

    cmd(CMD_SWRESET);             vTaskDelay(pdMS_TO_TICKS(150));
    cmd(CMD_SLPOUT);              vTaskDelay(pdMS_TO_TICKS(150));

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

    applyMadctl();
    cmd(CMD_COLMOD); data({0x55});       // 16-bit RGB565
    cmd(0x21);                           // INVON
    cmd(0xB1); data({0x00, 0x1B});       // frame rate ~70 Hz
    cmd(0xB6); data({0x08, 0x82, 0x27});

    cmd(0xF2); data({0x00});
    cmd(0x26); data({0x01});
    cmd(0xE0); data({0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                     0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00});
    cmd(0xE1); data({0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                     0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F});

    cmd(CMD_DISPON);              vTaskDelay(pdMS_TO_TICKS(100));
}

// MADCTL per rotation. BGR bit (0x08) kept so blitRgb565()'s R↔B swap stays valid.
void LcdDriver::applyMadctl() {
    if (!spiHandle_) return;
    static constexpr uint8_t kMadctl[4] = {0x48, 0x28, 0x88, 0xE8};
    uint8_t c = CMD_MADCTL;             spiWrite(&c, 1, false);
    uint8_t d = kMadctl[rotation_ & 3]; spiWrite(&d, 1, true);
}

void LcdDriver::setRotation(uint8_t r) {
    rotation_ = (uint8_t)(r & 3);
    const bool land = (rotation_ == 1 || rotation_ == 3);
    width_  = land ? nativeH_ : nativeW_;
    height_ = land ? nativeW_ : nativeH_;
    applyMadctl();
    fullFlush_ = true;
}

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

// ── Flush: expand 1-bit framebuf → RGB565, send in chunks ─────────────────
void LcdDriver::flush() {
    if (!framebuf_ || !spiHandle_) return;
    fullFlush_ = true;   // this path wrote the panel → invalidate flushBuffer diff

    setWindow(0, 0, (uint16_t)(width_ - 1), (uint16_t)(height_ - 1));

    for (uint16_t y0 = 0; y0 < height_; y0 += CHUNK_ROWS) {
        uint16_t rows = (uint16_t)((y0 + CHUNK_ROWS <= height_)
                                       ? CHUNK_ROWS : (height_ - y0));
        size_t n = 0;
        for (uint16_t y = y0; y < y0 + rows; y++) {
            for (uint16_t x = 0; x < width_; x++) {
                size_t idx = (size_t)y * width_ + x;
                bool   on  = (framebuf_[idx / 8] >> (7 - (idx % 8))) & 1;
                chunkbuf_[n++] = on ? fgColor_ : bgColor_;
            }
        }
        spiWrite((uint8_t*)chunkbuf_, n * 2, true);
    }
}

// ── Fast mono blit: 1-bit packed buffer → RGB565 → SPI, single pass ──────
// AppHost path for fullscreen apps. `buf` is 1-bit PACKED (nema::mono1, MSB-first,
// same layout as framebuf_), so the row diff and expansion just read bits. Rows
// are byte-aligned when width_ is a multiple of 8 (240/320 here); otherwise we
// fall back to a full repaint.
void LcdDriver::flushBuffer(const uint8_t* buf, uint16_t w, uint16_t h) {
    if (!buf || !spiHandle_) return;
    if (w != width_ || h != height_) return;   // full-screen only for now

    const bool   byteAligned = (width_ % 8) == 0;
    const size_t rowBytes    = (size_t)width_ / 8;
    const bool   full        = fullFlush_ || !prevBuf_ || !byteAligned;
    fullFlush_ = false;

    auto rowChanged = [&](uint16_t y) -> bool {
        if (full || !prevBuf_) return true;
        const uint8_t* a = buf      + (size_t)y * rowBytes;
        const uint8_t* b = prevBuf_ + (size_t)y * rowBytes;
        return std::memcmp(a, b, rowBytes) != 0;
    };

    uint16_t y = 0;
    while (y < height_) {
        if (!rowChanged(y)) { y++; continue; }

        uint16_t y0 = y;
        while (y < height_ && (uint16_t)(y - y0) < CHUNK_ROWS && rowChanged(y)) y++;
        uint16_t rows = (uint16_t)(y - y0);

        size_t n = 0;
        for (uint16_t ry = y0; ry < y0 + rows; ry++) {
            for (uint16_t x = 0; x < width_; x++) {
                size_t idx = (size_t)ry * width_ + x;
                bool   on  = (buf[idx >> 3] >> (7 - (idx & 7))) & 1;
                chunkbuf_[n++] = on ? fgColor_ : bgColor_;
            }
            if (prevBuf_ && byteAligned)
                std::memcpy(prevBuf_ + (size_t)ry * rowBytes, buf + (size_t)ry * rowBytes, rowBytes);
        }
        setWindow(0, y0, (uint16_t)(width_ - 1), (uint16_t)(y0 + rows - 1));
        spiWrite((uint8_t*)chunkbuf_, n * 2, true);
    }
}

// ── Direct RGB565 blit ────────────────────────────────────────────────────
// Standard RGB565 source; ILI9341 is in BGR mode (MADCTL D3=1) so swap R↔B.
void LcdDriver::blitRgb565(const uint8_t* buf,
                             uint16_t x, uint16_t y,
                             uint16_t w, uint16_t h) {
    if (!spiHandle_ || !buf || w == 0 || h == 0) return;
    setWindow(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));

    for (uint16_t row = 0; row < h; ) {
        uint16_t batch  = (uint16_t)((h - row) < (uint16_t)CHUNK_ROWS
                                     ? (h - row) : (uint16_t)CHUNK_ROWS);
        size_t   pixels = (size_t)batch * w;

        for (size_t i = 0; i < pixels; i++) {
            size_t   idx = (((size_t)row * w) + i) * 2;
            uint16_t rgb = ((uint16_t)buf[idx] << 8) | buf[idx + 1];
            uint8_t  r   = (rgb >> 11) & 0x1F;
            uint8_t  g   = (rgb >>  5) & 0x3F;
            uint8_t  b   =  rgb         & 0x1F;
            uint16_t bgr = ((uint16_t)b << 11) | ((uint16_t)g << 5) | r;
            chunkbuf_[i] = bgr;
        }
        spiWrite((uint8_t*)chunkbuf_, pixels * 2, true);
        row = (uint16_t)(row + batch);
    }
}

// ── Backlight + SPI ───────────────────────────────────────────────────────
void LcdDriver::setBacklight(bool on) {
    gpio_set_level((gpio_num_t)PIN_LCD_BL, on ? 1 : 0);
}

void LcdDriver::spiWrite(uint8_t* data, size_t len, bool isData) {
    if (!len) return;
    gpio_set_level((gpio_num_t)PIN_LCD_DC, isData ? 1 : 0);
    spi_transaction_t t = {};
    t.length    = len * 8;
    t.tx_buffer = data;
    spi_device_transmit((spi_device_handle_t)spiHandle_, &t);
}

} // namespace nema::skyrizzsolana
