#include "kairo/screens/camera_settings_screen.h"
#include "kairo/runtime.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/ui/components.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/services/camera_service.h"
#include "kairo/input/input_action.h"
#include <cstdio>

namespace kairo {

CameraSettingsScreen::CameraSettingsScreen(Runtime& rt) : rt_(rt) {}

void CameraSettingsScreen::enter() {
    cursor_ = 0;
    rt_.view().requestRedraw();
}

void CameraSettingsScreen::update(Key key) {
    switch (key) {
    case Key::Up:
        if (cursor_ > 0) cursor_--;
        break;
    case Key::Down:
        if (cursor_ < rt_.camera().count() - 1) cursor_++;
        break;
    case Key::Cancel:
        rt_.view().pop();
        return;
    default:
        break;
    }
    rt_.view().requestRedraw();
}

void CameraSettingsScreen::draw(Canvas& c) {
    uint16_t y = ui::drawTitle(c, "CAMERA");

    if (rt_.camera().count() == 0) {
        c.drawText(5, y, "No camera hardware", true);
    } else {
        for (int i = 0; i < rt_.camera().count(); i++) {
            bool sel = (cursor_ == i);
            auto* cam = rt_.camera().get(i);
            char line[40];
            std::snprintf(line, sizeof(line), "%s  %dx%d",
                rt_.camera().desc(i),
                (int)cam->frameWidth(), (int)cam->frameHeight());
            if (sel) c.invertRect(2, (uint16_t)(y - 1), (uint16_t)(c.width() - 4), (uint16_t)(ui::CHAR_H + 1));
            c.drawText(5, y, line, !sel);
            y += ui::CHAR_H + 4;
        }
    }

    char footer[56];
    std::snprintf(footer, sizeof(footer), "%s back",
        rt_.input().hintFor(input::Action::Back));
    c.drawText(4, ui::footerY(c.height()), footer, true);
}

} // namespace kairo
