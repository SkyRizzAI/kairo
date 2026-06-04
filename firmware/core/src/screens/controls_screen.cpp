#include "kairo/screens/controls_screen.h"
#include "kairo/runtime.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/ui/components.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/services/input_service.h"
#include "kairo/input/i_key_map.h"
#include "kairo/service/service_container.h"
#include "kairo/config/config_store.h"
#include <cstdio>

namespace kairo {

ControlsScreen::ControlsScreen(Runtime& rt) : rt_(rt) {}

void ControlsScreen::enter() {
    scroll_ = 0;
    rt_.view().requestRedraw();
}

void ControlsScreen::onAction(input::Action a) {
    switch (a) {
        case input::Action::Prev:
            if (scroll_ > 0) scroll_--;
            break;
        case input::Action::Next:
            scroll_++;
            break;
        case input::Action::Back:
            rt_.view().pop();
            return;
        default: break;
    }
    rt_.view().requestRedraw();
}

void ControlsScreen::draw(Canvas& c) {
    uint16_t y = ui::drawTitle(c, "CONTROLS");

    auto& input = rt_.input();
    auto* km    = input.keyMap();

    // Board info
    char line[48];
    std::snprintf(line, sizeof(line), "Board: %s", km ? km->boardName() : "simulator");
    c.drawText(4, y, line, true); y += ui::CHAR_H + 2;

    if (km) {
        std::snprintf(line, sizeof(line), "Buttons: %d", km->buttonCount());
        c.drawText(4, y, line, true); y += ui::CHAR_H + 2;
    }

    c.fillRect(4, y, c.width() - 8, 1, true); y += 3;

    // Action → hint table
    c.drawText(4, y, "ACTIONS", true); y += ui::CHAR_H + 1;

    static const input::Action kActions[] = {
        input::Action::Prev, input::Action::Next,
        input::Action::Activate, input::Action::Back,
        input::Action::AdjustUp, input::Action::AdjustDown,
        input::Action::Menu,
    };
    static constexpr int kActionCount = 7;

    int startRow = scroll_;
    int row = 0;
    for (int i = 0; i < kActionCount; i++) {
        if (row < startRow) { row++; continue; }
        if (y + ui::CHAR_H > ui::sep2Y(c.height())) break;
        input::Action a = kActions[i];
        const char* hint = input.hintFor(a);
        bool reachable   = input.canReach(a);
        std::snprintf(line, sizeof(line), "  %-10s %s%s",
            input::actionName(a),
            hint[0] ? hint : "-",
            reachable ? "" : " (N/A)");
        c.drawText(4, y, line, true);
        y += ui::CHAR_H + 1;
        row++;
    }

    // Gesture timing
    if (y + ui::CHAR_H + 2 < ui::sep2Y(c.height())) {
        c.fillRect(4, y, c.width() - 8, 1, true); y += 3;
        c.drawText(4, y, "GESTURES", true); y += ui::CHAR_H + 1;

        uint32_t longMs   = 500;
        uint32_t chordMs  = 80;
        if (auto* cfg = rt_.container().resolve<IConfigStore>()) {
            longMs  = (uint32_t)cfg->getIntOr("input", "long_ms",  (int64_t)longMs);
            chordMs = (uint32_t)cfg->getIntOr("input", "chord_ms", (int64_t)chordMs);
        }
        std::snprintf(line, sizeof(line), "  Long  >= %ums", (unsigned)longMs);
        c.drawText(4, y, line, true); y += ui::CHAR_H + 1;
        std::snprintf(line, sizeof(line), "  Short <  %ums", (unsigned)longMs);
        c.drawText(4, y, line, true);
    }

    // Footer
    char footer[48];
    std::snprintf(footer, sizeof(footer), "%s/%s scroll  %s back",
        rt_.input().hintFor(input::Action::Prev),
        rt_.input().hintFor(input::Action::Next),
        rt_.input().hintFor(input::Action::Back));
    c.drawText(4, ui::footerY(c.height()), footer, true);
}

} // namespace kairo
