#include "nema/ui/fbcon_server.h"
#include "nema/ui/canvas.h"
#include "nema/ui/ui_constants.h"
#include "nema/runtime.h"
#include "nema/system/system_info.h"
#include "nema/system/capability_registry.h"
#include "nema/clock.h"
#include <cstdio>
#include <string>

namespace nema {

void FbconServer::renderFrame(Canvas& c, ViewDispatcher&, const StatusBarData&) {
    c.clear();
    const uint16_t lh = ui::CHAR_H + 1;
    uint16_t y = 1;
    auto line = [&](const std::string& s) {
        if (y + lh <= c.height()) c.drawText(1, y, s.c_str(), true);
        y += lh;
    };

    const SystemInfo& info = rt_.info();
    line(info.boardName + " console [fbcon]");
    line("fw " + info.firmwareVersion + "  " + info.platformName);

    uint64_t ms = rt_.clock().millis();
    char up[40];
    std::snprintf(up, sizeof(up), "uptime %um %us",
                  (unsigned)(ms / 60000), (unsigned)((ms / 1000) % 60));
    line(up);

    const auto& caps = rt_.capabilities().list();
    line("caps (" + std::to_string(caps.size()) + "):");
    // Pack capabilities into wrapped lines so the console reads like a terminal.
    std::string row;
    for (const auto& cap : caps) {
        std::string next = row.empty() ? cap : row + " " + cap;
        if (c.textWidth(next.c_str()) > c.width() - 2) { line(row); row = cap; }
        else                                            row = next;
    }
    if (!row.empty()) line(row);

    line("> display start pixelate");   // hint: how to bring up the UI

    c.flush();
}

} // namespace nema
