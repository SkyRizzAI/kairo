#include "nema/devboard/eink_display.h"
#include "nema/devboard/board_config.h"
#include "nema/log/logger.h"
#include <GxEPD2_BW.h>
#include <gdey/GxEPD2_270_GDEY027T91.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <cstring>

namespace nema {

using namespace devboard;

static GxEPD2_BW<GxEPD2_270_GDEY027T91, GxEPD2_270_GDEY027T91::HEIGHT>
    g_epd(GxEPD2_270_GDEY027T91(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));

void EinkDisplay::init(Logger& log) { log_ = &log; }

void EinkDisplay::start() {
    SPI.begin(PIN_EPD_SCK, /*MISO*/ -1, PIN_EPD_MOSI, PIN_EPD_CS);
    g_epd.init(115200, true, 10, false);
    g_epd.setRotation(1);

    // Big buffers live in PSRAM — keep internal SRAM for stacks/WiFi.
    const size_t n = (size_t)W * H;
    buf_      = (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
    prev_buf_ = (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
    if (!buf_)      buf_      = (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_8BIT);
    if (!prev_buf_) prev_buf_ = (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_8BIT);
    std::memset(buf_, 0, n);
    std::memset(prev_buf_, 1, n);  // force a full refresh on first flush

    // Initial clear — synchronous, runs once at boot before the async task exists.
    flushBuffer(buf_, W, H);

    log_->info("EinkDisplay", "started (sync panel)",
        {{"panel", "GDEY027T91"}, {"res", "264x176"}});
}

void EinkDisplay::stop() {
    g_epd.powerOff();
    if (buf_)      { heap_caps_free(buf_);      buf_ = nullptr; }
    if (prev_buf_) { heap_caps_free(prev_buf_); prev_buf_ = nullptr; }
    log_->info("EinkDisplay", "stopped");
}

// ── Standalone draw path (buf_) — unused when wrapped by AsyncDisplayDriver ──

void EinkDisplay::drawPixel(uint16_t x, uint16_t y, bool on) {
    if (x >= W || y >= H || !buf_) return;
    buf_[y * W + x] = on ? 1 : 0;
}

void EinkDisplay::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool on) {
    if (!buf_) return;
    for (uint16_t row = y; row < y + h && row < H; row++) {
        uint16_t x1 = x < W ? x : W;
        uint16_t x2 = x + w < W ? x + w : W;
        if (x1 < x2) std::memset(buf_ + row * W + x1, on ? 1 : 0, x2 - x1);
    }
}

void EinkDisplay::clear(bool on) {
    if (buf_) std::memset(buf_, on ? 1 : 0, (size_t)W * H);
}

void EinkDisplay::invertRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!buf_) return;
    for (uint16_t row = y; row < y + h && row < H; row++)
        for (uint16_t col = x; col < x + w && col < W; col++)
            buf_[row * W + col] ^= 1;
}

void EinkDisplay::flush() { flushBuffer(buf_, W, H); }

// ── Primary path: dirty-rect diff + partial/full refresh + GxEPD2 SPI ──────

void EinkDisplay::flushBuffer(const uint8_t* buf, uint16_t w, uint16_t h) {
    if (!buf || !prev_buf_) return;

    // Dirty bounding box vs last-sent state.
    uint16_t x0 = w, y0 = h, x1 = 0, y1 = 0;
    for (uint16_t y = 0; y < h; y++) {
        for (uint16_t x = 0; x < w; x++) {
            if (buf[y * w + x] != prev_buf_[y * w + x]) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
        }
    }
    if (x1 < x0) return;  // nothing changed

    uint32_t dirty = (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1);
    uint32_t total = (uint32_t)w * h;
    bool do_full = (dirty > total * 3 / 4) || (++partial_count_ >= FULL_REFRESH_EVERY);

    if (do_full) {
        partial_count_ = 0;
        g_epd.setFullWindow();
    } else {
        uint16_t px = x0 & ~uint16_t{7};                  // byte-align to 8px
        uint16_t pw = ((x1 + 8) & ~uint16_t{7}) - px;
        g_epd.setPartialWindow(px, y0, pw, y1 - y0 + 1);
    }

    g_epd.firstPage();
    do {
        for (uint16_t y = 0; y < h; y++)
            for (uint16_t x = 0; x < w; x++)
                g_epd.drawPixel(x, y, buf[y * w + x] ? GxEPD_BLACK : GxEPD_WHITE);
    } while (g_epd.nextPage());

    std::memcpy(prev_buf_, buf, (size_t)w * h);
}

} // namespace nema
