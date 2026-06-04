#include "kairo/screens/about_screen.h"
#include "kairo/runtime.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/ui/components.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/system/system_info.h"
#include "kairo/system/capability_registry.h"
#include "kairo/clock.h"
#include <cstdio>

namespace kairo {

AboutScreen::AboutScreen(Runtime& rt) : rt_(rt) {}

void AboutScreen::enter() { rt_.view().requestRedraw(); }

void AboutScreen::update(Key key) {
    if (key == Key::Cancel) rt_.view().pop();
    rt_.view().requestRedraw();
}

void AboutScreen::draw(Canvas& c) {
    // Normal mode: runtime already cleared canvas + drew the status bar.
    uint16_t y = ui::drawTitle(c, "ABOUT");

    const auto& info = rt_.info();
    char buf[48];

    if (!info.boardName.empty()) {
        std::snprintf(buf, sizeof(buf), "Board:  %s", info.boardName.c_str());
        c.drawText(4, y, buf); y += ui::CHAR_H;
    }
    if (!info.platformName.empty()) {
        std::snprintf(buf, sizeof(buf), "Plat:   %s", info.platformName.c_str());
        c.drawText(4, y, buf); y += ui::CHAR_H;
    }

    std::snprintf(buf, sizeof(buf), "FW:     %s", info.firmwareVersion.c_str());
    c.drawText(4, y, buf); y += ui::CHAR_H;

    uint64_t ms = rt_.clock().millis();
    uint32_t s  = (uint32_t)(ms / 1000) % 60;
    uint32_t m  = (uint32_t)(ms / 60000) % 60;
    uint32_t h  = (uint32_t)(ms / 3600000);
    if (h > 0)
        std::snprintf(buf, sizeof(buf), "Up:     %uh %um %us", (unsigned)h, (unsigned)m, (unsigned)s);
    else
        std::snprintf(buf, sizeof(buf), "Up:     %um %us", (unsigned)m, (unsigned)s);
    c.drawText(4, y, buf); y += ui::CHAR_H + 3;

    // Capabilities list
    c.fillRect(4, y, c.width() - 8, 1); y += 4;
    c.drawText(4, y, "Caps:"); y += ui::CHAR_H;
    for (const auto& cap : rt_.capabilities().list()) {
        if (y + ui::CHAR_H > c.height() - 4) break;
        std::snprintf(buf, sizeof(buf), "  %s", cap.c_str());
        c.drawText(4, y, buf); y += ui::CHAR_H;
    }
}

} // namespace kairo
