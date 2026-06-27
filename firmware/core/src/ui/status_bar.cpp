// Plan 60 Fase 3 — status bar rewrite.
// Plan 82 Phase 3 — replaced hand-drawn icons with Flipper Zero T1 assets.
#include "nema/ui/status_bar.h"
#include "nema/ui/canvas.h"
#include "nema/ui/draw.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/style_tokens.h"
#include "nema/assets/system_icons.h"

namespace nema {

static void drawIcon(Canvas& c, uint16_t x, uint16_t y, const nema::assets::Icon& ic, bool inv) {
    // inv=true → status bar is filled (ON), so icon 1-bits must be drawn OFF to
    // produce a visible silhouette. inv=false → plain dark background, draw ON.
    for (uint8_t row = 0; row < ic.h; row++) {
        for (uint8_t col = 0; col < ic.w; col++) {
            uint32_t bit = (uint32_t)row * ic.w + col;
            bool on = (ic.data[bit / 8] >> (7 - (bit % 8))) & 1;
            if (on) c.drawPixel((uint16_t)(x + col), (uint16_t)(y + row), !inv);
        }
    }
}

// Tiny 3×5 digit glyphs (rows top→bottom, 3 bits each, MSB = leftmost pixel).
static const uint8_t kTinyDigit[10][5] = {
    {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
    {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
    {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
    {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
    {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
    {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
    {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
    {0b111, 0b001, 0b010, 0b010, 0b010}, // 7
    {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
    {0b111, 0b101, 0b111, 0b001, 0b111}, // 9
};

static uint16_t drawTinyDigit(Canvas& c, uint16_t x, uint16_t y, int d, bool on) {
    const uint8_t* g = kTinyDigit[d % 10];
    for (int r = 0; r < 5; r++)
        for (int col = 0; col < 3; col++)
            if (g[r] & (1 << (2 - col))) c.drawPixel((uint16_t)(x + col), (uint16_t)(y + r), on);
    return (uint16_t)(x + 4);   // 3 wide + 1 gap
}

void StatusBar::drawClock(Canvas& c, uint16_t x, uint16_t y, int h, int m, bool on) {
    x = drawTinyDigit(c, x, y, h / 10, on);
    x = drawTinyDigit(c, x, y, h % 10, on);
    c.drawPixel(x, (uint16_t)(y + 1), on);   // colon (1px wide)
    c.drawPixel(x, (uint16_t)(y + 3), on);
    x += 2;
    x = drawTinyDigit(c, x, y, m / 10, on);
    drawTinyDigit(c, x, y, m % 10, on);
}

void StatusBar::draw(Canvas& c, const StatusBarData& d) {
    using namespace nema::assets;
    const aether::StyleTokens& t = aether::theme();

    bool inv  = t.invertedStatusBar;
    uint16_t barH = (uint16_t)(nema::display::STATUS_H + 2);

    if (inv) c.fillRect(0, 0, c.width(), barH);

    bool textOn = !inv;

    // Clock — top-left, tiny 3×5 glyphs (Plan 92 Fase D: smaller than the 8px fonts).
    uint16_t cty = (barH > 5) ? (uint16_t)((barH - 5) / 2) : 0;
    drawClock(c, 2, cty, d.hour, d.minute, textOn);

    // Right side — icons packed left from right edge
    uint16_t rx = (uint16_t)(c.width() - 2);

    auto iconY = [&](uint8_t h) -> uint16_t {
        return (barH > h) ? (uint16_t)((barH - h) / 2) : 0;
    };
    auto placeIcon = [&](const Icon& ic) {
        rx = (rx > ic.w + 2u) ? (uint16_t)(rx - ic.w - 2u) : 0u;
        drawIcon(c, rx, iconY(ic.h), ic, inv);
    };

    // Battery — simplified ~11px hand-drawn (Plan 92 Fase D), replaces 25px icon.
    constexpr uint16_t kBatW = 11, kBatH = 6;
    rx = (rx > kBatW + 2u) ? (uint16_t)(rx - kBatW - 2u) : 0u;
    drawBattery(c, rx, iconY((uint8_t)kBatH), d.battery, textOn);

    // WiFi — standing signal bars (Plan 92 Fase D): clean + short, no clipping.
    if (d.wifi) {
        constexpr uint16_t kWifiW = 7, kWifiH = 5;
        rx = (rx > kWifiW + 2u) ? (uint16_t)(rx - kWifiW - 2u) : 0u;
        drawWifi(c, rx, iconY((uint8_t)kWifiH), textOn);
    }

    // BLE
    if (d.ble) placeIcon(icBleIdle);

    // SD card
    if (d.sdMounted) placeIcon(icSdMounted);

    // Lock / hidden window
    if (d.locked) placeIcon(icLock);

    // No bottom separator/border — the status bar sits directly above content.
}

// WiFi — 4 standing signal bars (1px wide, heights 2/3/4/5), bottom-aligned. 7×5.
void StatusBar::drawWifi(Canvas& c, uint16_t x, uint16_t y, bool on) {
    static const uint8_t bh[4] = {2, 3, 4, 5};
    for (int i = 0; i < 4; i++)
        c.fillRect((uint16_t)(x + i * 2), (uint16_t)(y + 5 - bh[i]), 1, bh[i], on);
}

// Simplified battery: 10×6 body + 1px nub, inner fill proportional to pct.
void StatusBar::drawBattery(Canvas& c, uint16_t x, uint16_t y, int pct, bool on) {
    constexpr uint16_t bw = 10, bh = 6;
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    c.drawRect(x, y, bw, bh, on);                                  // body outline
    c.fillRect((uint16_t)(x + bw), (uint16_t)(y + 2), 1, 2, on);   // + terminal nub
    int fillW = (pct * (bw - 2)) / 100;                            // inner charge bar
    if (fillW > 0)
        c.fillRect((uint16_t)(x + 1), (uint16_t)(y + 1), (uint16_t)fillW, (uint16_t)(bh - 2), on);
}

} // namespace nema
