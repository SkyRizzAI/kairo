#include "kairo/screens/settings_screen.h"
#include "kairo/runtime.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/ui_constants.h"
#include "kairo/ui/components.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/app/app_host.h"
#include "kairo/system/capability_registry.h"
#include <cstring>
#include <cstdio>

namespace kairo {

SettingsScreen::SettingsScreen(Runtime& rt)
    : rt_(rt), about_(rt), sleepSettings_(rt), controls_(rt), touchSettings_(rt),
      sounds_(rt), cameraSettings_(rt) {}
SettingsScreen::~SettingsScreen() = default;

void SettingsScreen::buildMenu() {
    items_.clear();
    // Connectivity — only shown when the board actually supports it
    if (rt_.capabilities().has("wifi"))       items_.push_back({"WiFi"});
    if (rt_.capabilities().has("bluetooth"))  items_.push_back({"Bluetooth"});
    // Always present
    items_.push_back({"Display"});
    items_.push_back({"Controls"});
    // Touch submenu — only on boards with a touch controller
    if (rt_.capabilities().has("input.touch")) items_.push_back({"Touch"});
    if (rt_.capabilities().has("audio.input") || rt_.capabilities().has("audio.output"))
        items_.push_back({"Sounds"});
    if (rt_.capabilities().has("camera"))
        items_.push_back({"Camera"});
    items_.push_back({"About"});
    if (cursor_ >= (int)items_.size()) cursor_ = 0;
}

void SettingsScreen::enter() {
    buildMenu();
    rt_.view().requestRedraw();
}

void SettingsScreen::handleSelect() {
    if (cursor_ < 0 || cursor_ >= (int)items_.size()) return;
    const char* label = items_[cursor_].label;
    if (std::strcmp(label, "About") == 0) {
        rt_.view().push(about_);
    } else if (std::strcmp(label, "Display") == 0) {
        rt_.view().push(sleepSettings_);
    } else if (std::strcmp(label, "Controls") == 0) {
        rt_.view().push(controls_);
    } else if (std::strcmp(label, "WiFi") == 0) {
        // Launch WiFi as a true app on its own thread.
        wifiHost_ = std::make_unique<AppHost>(rt_, wifiApp_);
        rt_.view().push(*wifiHost_);
    } else if (std::strcmp(label, "Touch") == 0) {
        rt_.view().push(touchSettings_);
    } else if (std::strcmp(label, "Sounds") == 0) {
        rt_.view().push(sounds_);
    } else if (std::strcmp(label, "Camera") == 0) {
        rt_.view().push(cameraSettings_);
    }
}

void SettingsScreen::update(Key key) {
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

void SettingsScreen::draw(Canvas& c) {
    // Normal mode: runtime already cleared canvas + drew the status bar.
    // Do NOT clear() here — it would wipe the status bar.
    uint16_t y = ui::drawTitle(c, "SETTINGS");
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
}

} // namespace kairo
