#include "kairo/screens/home_screen.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/runtime.h"
#include <cstdio>

namespace kairo {

const char* HomeScreen::MENU_LABELS[] = {"Apps", "Logs", "Settings"};

HomeScreen::HomeScreen(Runtime& rt)
    : rt_(rt), appList_(rt), logs_(rt), settings_(rt) {}

void HomeScreen::enter()  { rt_.view().requestRedraw(); }
void HomeScreen::tick(uint64_t) {}

void HomeScreen::update(Key key) {
    switch (key) {
        case Key::Up:   if (cursor_ > 0) cursor_--;             break;
        case Key::Down: if (cursor_ < MENU_SIZE - 1) cursor_++; break;
        case Key::Select:
            switch (cursor_) {
                case 0: rt_.view().push(appList_);  return;
                case 1: rt_.view().push(logs_);     return;
                case 2: rt_.view().push(settings_); return;
            }
            break;
        default: break;
    }
    rt_.view().requestRedraw();
}

void HomeScreen::drawMenu(Canvas& c) {
    uint16_t menu_y = c.height() - (uint16_t)(MENU_SIZE * ui::CHAR_H) - 10;
    for (int i = 0; i < MENU_SIZE; i++) {
        bool sel = (i == cursor_);
        uint16_t row_y = menu_y + (uint16_t)(i * ui::CHAR_H);
        char line[24];
        std::snprintf(line, sizeof(line), "> %s", MENU_LABELS[i]);
        if (sel) {
            uint16_t hw = c.textWidth(line) + 6;
            c.invertRect(2, row_y - 1, hw, ui::CHAR_H + 1);
        } else {
            std::snprintf(line, sizeof(line), "  %s", MENU_LABELS[i]);
        }
        c.drawText(5, row_y, line, !sel);
    }
}

void HomeScreen::draw(Canvas& c) {
    // Status bar drawn automatically by runtime (Normal mode)
    constexpr uint8_t LOGO_SCALE = 7;
    uint16_t lx = c.centerXScaled("KAIRO", LOGO_SCALE);
    c.drawTextScaled(lx, ui::CONTENT_Y + 14, "KAIRO", LOGO_SCALE);
    drawMenu(c);
}

} // namespace kairo
