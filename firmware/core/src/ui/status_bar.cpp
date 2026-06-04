#include "kairo/ui/status_bar.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/ui_constants.h"
#include <cstdio>

namespace kairo {

void StatusBar::draw(Canvas& c, const StatusBarData& d) {
    using namespace kairo::ui;

    // Clock — left
    char clk[6];
    std::snprintf(clk, sizeof(clk), "%02d:%02d", d.hour, d.minute);
    c.drawText(2, STATUS_Y, clk);

    // Right side: retro text indicators  "W  BT [100%]" or "BT [100%]"
    char right[24];
    if (d.wifi)
        std::snprintf(right, sizeof(right), "W  BT [%d%%]", d.battery);
    else
        std::snprintf(right, sizeof(right), "BT [%d%%]", d.battery);
    uint16_t rw = c.textWidth(right);
    uint16_t rx = (c.width() > rw + 2) ? c.width() - rw - 2 : 0;
    c.drawText(rx, STATUS_Y, right);

    // Separator line below status bar
    c.fillRect(0, SEP1_Y + 1, c.width(), 1);
}

} // namespace kairo
