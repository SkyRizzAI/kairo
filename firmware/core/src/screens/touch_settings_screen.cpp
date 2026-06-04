#include "kairo/screens/touch_settings_screen.h"
#include "kairo/runtime.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/ui/components.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/services/input_service.h"
#include "kairo/input/input_action.h"
#include "kairo/app/app_host.h"
#include <cstring>
#include <cstdio>

namespace kairo {

TouchSettingsScreen::TouchSettingsScreen(Runtime& rt) : rt_(rt) {}
TouchSettingsScreen::~TouchSettingsScreen() = default;

void TouchSettingsScreen::enter() {
    items_.clear();
    items_.push_back({"Touch Test"});
    // future: "Calibrate", "Sensitivity", …
    cursor_ = 0;
    rt_.view().requestRedraw();
}

void TouchSettingsScreen::handleSelect() {
    if (cursor_ < 0 || cursor_ >= (int)items_.size()) return;
    const char* label = items_[cursor_].label;
    if (std::strcmp(label, "Touch Test") == 0) {
        touchHost_ = std::make_unique<AppHost>(rt_, touchApp_);
        rt_.view().push(*touchHost_);
    }
}

void TouchSettingsScreen::update(Key key) {
    int sz = (int)items_.size();
    switch (key) {
        case Key::Up:     if (cursor_ > 0)      cursor_--; break;
        case Key::Down:   if (cursor_ < sz - 1) cursor_++; break;
        case Key::Select: handleSelect(); return;
        case Key::Cancel: rt_.view().pop(); return;
        default: break;
    }
    rt_.view().requestRedraw();
}

void TouchSettingsScreen::draw(Canvas& c) {
    uint16_t y = ui::drawTitle(c, "TOUCH");
    for (int i = 0; i < (int)items_.size(); i++) {
        bool sel = (i == cursor_);
        uint16_t row_y = y + (uint16_t)(i * ui::CHAR_H);
        char line[32];
        std::snprintf(line, sizeof(line), "> %s", items_[i].label);
        if (sel) {
            uint16_t hw = c.textWidth(line) + 6;
            c.invertRect(2, row_y - 1, hw, ui::CHAR_H + 1);
        } else {
            std::snprintf(line, sizeof(line), "  %s", items_[i].label);
        }
        c.drawText(5, row_y, line, !sel);
    }
    char footer[48];
    std::snprintf(footer, sizeof(footer), "%s back",
                  rt_.input().hintFor(input::Action::Back));
    c.drawText(4, ui::footerY(c.height()), footer, true);
}

} // namespace kairo
