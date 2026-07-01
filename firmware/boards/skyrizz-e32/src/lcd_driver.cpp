#include "nema/skyrizze32/lcd_driver.h"
#include "nema/skyrizze32/xl9535.h"
#include "nema/skyrizze32/board_config.h"
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

namespace nema::skyrizze32 {

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
// chunks instead of per-row cuts SPI transactions/frame. 16 rows → ~20 txns/frame
// (vs ~10 at 32 rows): a small flush-speed cost for ~10 KB of internal SRAM freed
// for the BLE controller, which MUST live in internal RAM (Plan 93). The bottleneck
// is per-transaction overhead, not bitrate. Sized for up to 320 px wide × 8 rows.
// 8 rows → ~40 txns/frame; chosen to free internal SRAM for the boot-reserved BLE
// controller (Plan 93). Bump back up if screen flush feels slow and RAM allows.
static constexpr int CHUNK_ROWS = 8;
static uint16_t chunkbuf_[320 * CHUNK_ROWS];

void LcdDriver::init(Runtime& rt, Xl9535& expander) {
    rt_       = &rt;
    expander_ = &expander;

    if (auto* cfg = rt.container().resolve<IConfigStore>()) {
        int w = (int)cfg->getIntOr("display", "width",  (int64_t)width_);
        int h = (int)cfg->getIntOr("display", "height", (int64_t)height_);
        if (w > 0 && w <= 1024) width_  = (uint16_t)w;
        if (h > 0 && h <= 1024) height_ = (uint16_t)h;
        nativeW_ = width_; nativeH_ = height_;   // portrait native (pre-rotation)

        // Display rotation (Plan 92 Fase A): 0/1/2/3 → 0°/90°/180°/270°.
        // 90°/270° are landscape — swap the logical framebuffer dims so the
        // resolution-independent UI reflows. The framebuffer byte size is
        // unchanged (w*h is identical), so start() allocates the same buffer;
        // panelInit() sets the matching ILI9341 MADCTL so the panel scans it
        // correctly. Touch is rotated in Ft6336Touch::toLogical().
        rotation_ = (uint8_t)(cfg->getIntOr("display", "rotation", 0) & 3);
        if (rotation_ == 1 || rotation_ == 3) {
            uint16_t t = width_; width_ = height_; height_ = t;
        }
        brightness_ = (uint8_t)cfg->getIntOr("display", "brightness", 255);
    }
}

// Backlight has no PWM on this board (XL9535 GPIO) — map level >0 → on, 0 → off.
void LcdDriver::setBrightness(uint8_t level) {
    brightness_ = level;
    setBacklight(level > 0);
}

void LcdDriver::start() {
    // 1-bit monochrome framebuffer. It is CPU-read into chunkbuf_ for the SPI DMA in
    // flush() — never DMA'd itself — so it lives in PSRAM, freeing ~10 KB internal SRAM
    // for the BLE controller + WiFi (Plan 93). Falls back to internal if PSRAM is absent.
    size_t fbSize = ((size_t)width_ * height_ + 7) / 8;
    framebuf_ = (uint8_t*)heap_caps_malloc(fbSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!framebuf_) framebuf_ = (uint8_t*)heap_caps_malloc(fbSize, MALLOC_CAP_8BIT);
    if (!framebuf_) {
        if (rt_) rt_->log().error("LcdDriver", "framebuf alloc failed");
        return;
    }
    std::memset(framebuf_, 0x00, fbSize);  // start black (on=false = background)

    // Previous-frame buffer for partial flush (row diff). Plan 97 P3b: the app
    // buffer handed to flushBuffer() is now 1-bit packed, so prevBuf_ matches
    // (same fbSize). PSRAM is fine — accessed sequentially (memcmp/memcpy).
    prevBuf_ = (uint8_t*)heap_caps_malloc(fbSize, MALLOC_CAP_SPIRAM);
    if (!prevBuf_) prevBuf_ = (uint8_t*)heap_caps_malloc(fbSize, MALLOC_CAP_8BIT);
    fullFlush_ = true;

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
    setBacklight(brightness_ > 0);   // honour persisted brightness (on/off here)

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

    applyMadctl();                       // CMD_MADCTL for the current rotation_
    cmd(CMD_COLMOD); data({0x55});       // 16-bit RGB565
    cmd(0x21);                           // INVON — invert display at hardware level
                                        // (eliminates vignette at panel edges)
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

// ── MADCTL for current rotation ───────────────────────────────────────────
// BGR bit (0x08) kept in every value so blitRgb565()'s R↔B swap stays valid.
// Adafruit-standard set: 0°=0x48 (MX|BGR) · 90°=0x28 (MV|BGR) · 180°=0x88
// (MY|BGR) · 270°=0xE8 (MX|MY|MV|BGR).
void LcdDriver::applyMadctl() {
    if (!spiHandle_) return;
    static constexpr uint8_t kMadctl[4] = {0x48, 0x28, 0x88, 0xE8};
    uint8_t c = CMD_MADCTL;            spiWrite(&c, 1, false);
    uint8_t d = kMadctl[rotation_ & 3]; spiWrite(&d, 1, true);
}

// ── Live rotation ─────────────────────────────────────────────────────────
// Swap logical dims (same framebuffer byte size, no realloc), re-send MADCTL so
// the panel re-scans, and force a full repaint. Touch follows via
// Ft6336Touch::setRotation(). NOTE: bench-untested on hardware; the boot-time
// path (config display/rotation read in init()) is the verified one.
void LcdDriver::setRotation(uint8_t r) {
    rotation_ = (uint8_t)(r & 3);
    const bool land = (rotation_ == 1 || rotation_ == 3);
    width_  = land ? nativeH_ : nativeW_;
    height_ = land ? nativeW_ : nativeH_;
    applyMadctl();
    fullFlush_ = true;
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

// ── Unified diffing push: 1-bit packed frame → RGB565 → SPI, changed rows only ──
// Plan 98 (P4): the row-diff used to live only in flushBuffer() (app fast path),
// so component screens — which draw into framebuf_ then call flush() — paid a full
// ~48ms panel push EVERY frame even when a single character changed. pushMono() is
// the shared engine: both flush() (framebuf_) and flushBuffer() (an app buffer)
// diff a 1-bit packed frame against prevBuf_ (the panel's last content) and send
// only the rows that actually changed. prevBuf_ is the single "what's on the glass"
// state, valid across both callers (only one runs per frame). `buf` layout is
// nema::mono1: contiguous bit idx y*width_+x, MSB-first — same as framebuf_.
// Rows are byte-aligned when width_ is a multiple of 8 (240/320 here); otherwise
// we fall back to a full repaint. fullFlush_ forces a full send after any path that
// touched the panel outside this diff (init, rotation, palette change, blitRgb565).
void LcdDriver::pushMono(const uint8_t* buf) {
    if (!buf || !spiHandle_) return;

    const bool   byteAligned = (width_ % 8) == 0;
    const size_t rowBytes    = (size_t)width_ / 8;          // valid when byteAligned
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

        // Gather a contiguous run of changed rows, capped at one chunk buffer.
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

// Component-screen path: push the internal framebuffer (diffed).
void LcdDriver::flush() { pushMono(framebuf_); }

// App fast path (fullscreen, unscaled): push the app's buffer directly (diffed),
// skipping the 76,800 per-pixel drawPixel calls + the internal framebuffer.
void LcdDriver::flushBuffer(const uint8_t* buf, uint16_t w, uint16_t h) {
    if (w != width_ || h != height_) return;   // full-screen only for now
    pushMono(buf);
}

// Plan 98: scaled fast path — expand a LOGICAL 1-bit buffer (lw×lh, nema::mono1) by
// integer `scale` to the physical panel, chunked by physical rows. Replaces the
// ~92ms per-pixel canvas blit at UI scale 2× with a single scaled SPI push. No mono
// diff (scaled apps are typically fullscreen-animating); invalidate the diff so the
// next component flush() repaints cleanly. `lx` advances every `scale` columns to
// avoid a per-pixel divide.
bool LcdDriver::flushBufferScaled(const uint8_t* buf, uint16_t lw, uint16_t lh, uint8_t scale) {
    if (!buf || !spiHandle_ || scale < 2) return false;
    if ((uint32_t)lw * scale != width_ || (uint32_t)lh * scale != height_) return false;

    setWindow(0, 0, (uint16_t)(width_ - 1), (uint16_t)(height_ - 1));
    for (uint16_t py0 = 0; py0 < height_; py0 += CHUNK_ROWS) {
        uint16_t rows = (uint16_t)((py0 + CHUNK_ROWS <= height_) ? CHUNK_ROWS : (height_ - py0));
        size_t n = 0;
        for (uint16_t py = py0; py < py0 + rows; py++) {
            size_t   rowBase = (size_t)(py / scale) * lw;   // logical row for this phys row
            uint16_t lx = 0; uint8_t sub = 0;
            for (uint16_t px = 0; px < width_; px++) {
                size_t idx = rowBase + lx;
                bool   on  = (buf[idx >> 3] >> (7 - (idx & 7))) & 1;
                chunkbuf_[n++] = on ? fgColor_ : bgColor_;
                if (++sub == scale) { sub = 0; lx++; }
            }
        }
        spiWrite((uint8_t*)chunkbuf_, n * 2, true);
    }
    fullFlush_ = true;   // prevBuf_ no longer matches the glass
    return true;
}

// ── Direct RGB565 blit (camera viewfinder) ───────────────────────────────
// GC2145 outputs standard RGB565. ILI9341 is in BGR mode (MADCTL=0x48, D3=1),
// so R and B must be swapped. Panel inversion is handled by INVON (0x21).
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
            chunkbuf_[i] = bgr;
        }
        spiWrite((uint8_t*)chunkbuf_, pixels * 2, true);
        row = (uint16_t)(row + batch);
    }
    // Plan 98: this path wrote RGB565 straight to the panel, bypassing the mono
    // diff — prevBuf_ no longer matches the glass, so force a full mono repaint
    // on the next pushMono() (else stale camera pixels linger under the UI).
    fullFlush_ = true;
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

} // namespace nema::skyrizze32
