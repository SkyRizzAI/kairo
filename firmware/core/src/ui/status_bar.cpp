// Plan 60 Fase 3 — status bar rewrite.
// Plan 82 Phase 3 — replaced hand-drawn icons with Flipper Zero T1 assets.
#include "nema/ui/status_bar.h"
#include "nema/ui/canvas.h"
#include "nema/ui/draw.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/text_style.h"
#include "nema/assets/system_icons.h"
#include <cstdio>

namespace nema {

static void drawIcon(Canvas& c, uint16_t x, uint16_t y, const nema::assets::Icon& ic, bool inv) {
    // inv=true → invert pixels (white icon on dark status bar)
    for (uint8_t row = 0; row < ic.h; row++) {
        for (uint8_t col = 0; col < ic.w; col++) {
            uint32_t bit = (uint32_t)row * ic.w + col;
            bool on = (ic.data[bit / 8] >> (7 - (bit % 8))) & 1;
            if (on != inv) c.drawPixel((uint16_t)(x + col), (uint16_t)(y + row));
        }
    }
}

void StatusBar::draw(Canvas& c, const StatusBarData& d) {
    using namespace nema::assets;
    const aether::StyleTokens& t = aether::theme();

    bool inv  = t.invertedStatusBar;
    uint16_t barH = (uint16_t)(nema::display::STATUS_H + 2);

    if (inv) c.fillRect(0, 0, c.width(), barH);

    bool textOn = !inv;

    // Clock — left side
    char clk[6];
    std::snprintf(clk, sizeof(clk), "%02d:%02d", d.hour, d.minute);
    uint16_t ty  = (barH > nema::display::CHAR_H) ? (uint16_t)((barH - nema::display::CHAR_H) / 2) : 0;
    aether::ui::FontSpec fs  = aether::ui::fontForRole(aether::ui::TextRole::Caption);
    c.setFont(fs.handle);
    c.drawText(2, ty, clk, textOn);

    // Right side — icons packed left from right edge
    uint16_t rx = (uint16_t)(c.width() - 2);

    auto iconY = [&](uint8_t h) -> uint16_t {
        return (barH > h) ? (uint16_t)((barH - h) / 2) : 0;
    };
    auto placeIcon = [&](const Icon& ic) {
        rx = (rx > ic.w + 2u) ? (uint16_t)(rx - ic.w - 2u) : 0u;
        drawIcon(c, rx, iconY(ic.h), ic, inv);
    };

    // Battery (25×8 Flipper icon)
    placeIcon(icBattery);

    // WiFi (10×8 placeholder; TODO: real icon)
    if (d.wifi) placeIcon(icWifiOn);

    // BLE
    if (d.ble) placeIcon(icBleIdle);

    // SD card
    if (d.sdMounted) placeIcon(icSdMounted);

    // Lock / hidden window
    if (d.locked) placeIcon(icLock);

    if (!inv)
        aether::ui::draw::separator(c, 0, barH, c.width(), true);
}

} // namespace nema
