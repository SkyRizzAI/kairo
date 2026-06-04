#include "kairo/screens/logs_screen.h"
#include "kairo/runtime.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/ui/components.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/plugin/plugin_manager.h"
#include "kairo/clock.h"
#include <cstdio>

namespace kairo {

LogsScreen::LogsScreen(Runtime& rt) : rt_(rt) {}

void LogsScreen::enter() { rt_.view().requestRedraw(); }

void LogsScreen::update(Key key) {
    if (key == Key::Cancel) rt_.view().pop();
    rt_.view().requestRedraw();
}

void LogsScreen::draw(Canvas& c) {
    // Normal mode: runtime already cleared canvas + drew the status bar.
    c.drawText(c.centerX("LOGS"), ui::CONTENT_Y, "LOGS");
    c.fillRect(0, ui::CONTENT_Y + ui::CHAR_H + 1, c.width(), 1);

    uint16_t y = ui::CONTENT_Y + ui::CHAR_H + 4;
    char buf[48];

    // Runtime stats available without MemorySink access
    uint64_t ms  = rt_.clock().millis();
    uint32_t s   = (uint32_t)(ms / 1000) % 60;
    uint32_t m   = (uint32_t)(ms / 60000) % 60;
    uint32_t h   = (uint32_t)(ms / 3600000);
    if (h > 0)
        std::snprintf(buf, sizeof(buf), "Uptime: %uh %um %us", (unsigned)h, (unsigned)m, (unsigned)s);
    else
        std::snprintf(buf, sizeof(buf), "Uptime: %um %us", (unsigned)m, (unsigned)s);
    c.drawText(4, y, buf); y += ui::CHAR_H;

    std::snprintf(buf, sizeof(buf), "Apps:   %d loaded",
        (int)rt_.plugins().plugins().size());
    c.drawText(4, y, buf); y += ui::CHAR_H * 2;
}

} // namespace kairo
