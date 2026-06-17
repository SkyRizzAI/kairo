// Plan 60 Fase 3 — status bar rewrite.
// Plan 53 — icon_pack XBM bitmaps used for wifi and battery indicators.
#include "nema/ui/status_bar.h"
#include "nema/ui/canvas.h"
#include "nema/ui/draw.h"
#include "nema/ui/icon_pack.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/text_style.h"
#include <cstdio>

namespace nema {

// Battery: 8×5 bar-graph drawn dynamically to show charge level.
// The icon_pack status.battery bitmap is used as the shell outline; the fill
// is drawn on top to represent percentage. This gives a live indicator rather
// than a static icon.
static void drawBatteryDynamic(Canvas& c, uint16_t x, uint16_t y, int pct) {
    const uint16_t BW = 8, BH = 5, CAP = 2;
    c.drawRect(x, y, BW, BH);
    c.fillRect((uint16_t)(x + BW), (uint16_t)(y + 1), CAP, (uint16_t)(BH - 2));
    if (pct > 0) {
        uint16_t fillW = (uint16_t)((uint32_t)(BW - 2) * pct / 100);
        if (fillW > 0)
            c.fillRect((uint16_t)(x + 1), (uint16_t)(y + 1), fillW, (uint16_t)(BH - 2), true);
    }
}

void StatusBar::draw(Canvas& c, const StatusBarData& d) {
    using namespace nema::ui;
    const nema::StyleTokens& t = nema::theme();

    bool inv = t.invertedStatusBar;
    uint16_t barH = (uint16_t)(STATUS_H + 2);

    if (inv) c.fillRect(0, 0, c.width(), barH);

    bool textOn = !inv;

    // Clock — left, vertically centered
    char clk[6];
    std::snprintf(clk, sizeof(clk), "%02d:%02d", d.hour, d.minute);
    uint16_t ty  = (barH > CHAR_H) ? (uint16_t)((barH - CHAR_H) / 2) : 0;
    uint16_t icy = (barH > 8)      ? (uint16_t)((barH - 8) / 2)      : 0;
    FontSpec fs  = fontForRole(TextRole::Caption);
    c.setFont(*fs.font);
    c.drawText(2, ty, clk, textOn);

    // Right side — work left from edge
    uint16_t rx = (uint16_t)(c.width() - 2);

    // Battery: dynamic bar-graph (10px wide including cap nub)
    rx = (uint16_t)(rx - 10);
    drawBatteryDynamic(c, rx, (uint16_t)(icy + 1), d.battery);

    // WiFi icon (Plan 53 icon_pack): 8×8 XBM bitmap, 2px gap before battery
    if (d.wifi) {
        const IconDef* wd = findIcon("status.wifi");
        if (wd) {
            rx = (uint16_t)(rx > wd->w + 2u ? rx - wd->w - 2u : 0u);
            aether::ui::draw::icon(c, rx, icy, wd->bitmap, wd->w, wd->h);
        } else {
            rx = (uint16_t)(rx > (uint16_t)(CHAR_W + 2) ? rx - CHAR_W - 2 : 0);
            c.drawText(rx, ty, "W", textOn);
        }
    }

    if (!inv)
        aether::ui::draw::separator(c, 0, barH, c.width(), true);
}

} // namespace nema
