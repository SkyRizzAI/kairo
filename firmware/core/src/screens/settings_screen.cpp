#include "kairo/screens/settings_screen.h"
#include "kairo/runtime.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/app/app_host_manager.h"
#include "kairo/system/capability_registry.h"

namespace kairo {

using namespace ui;

SettingsScreen::SettingsScreen(Runtime& rt)
    : ComponentScreen(rt, 160), about_(rt), sleepSettings_(rt), controls_(rt),
      touchSettings_(rt), sounds_(rt), cameraSettings_(rt) {}

void SettingsScreen::enter() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    rt_.view().requestRedraw();
}

void SettingsScreen::onSelect(void* u) {
    auto* it = static_cast<Item*>(u);
    it->self->launch(it->kind);
}

void SettingsScreen::launch(Kind k) {
    switch (k) {
        case About:      rt_.view().push(about_);          break;
        case Display:    rt_.view().push(sleepSettings_);  break;
        case Controls:   rt_.view().push(controls_);       break;
        case Touch:      rt_.view().push(touchSettings_);  break;
        case Sounds:     rt_.view().push(sounds_);         break;
        case Camera:     rt_.view().push(cameraSettings_); break;
        case WiFi:       rt_.apps().launch(wifiApp_);      break;
        case Bluetooth:  rt_.apps().launch(bluetoothApp_); break;
    }
}

UiNode* SettingsScreen::build(NodeArena& a, Runtime& rt) {
    items_.clear();
    auto& caps = rt.capabilities();
    if (caps.has("wifi"))                                items_.push_back({this, WiFi,      "WiFi"});
    if (caps.has("bluetooth"))                           items_.push_back({this, Bluetooth, "Bluetooth"});
    items_.push_back({this, Display,  "Display"});
    items_.push_back({this, Controls, "Controls"});
    if (caps.has("input.touch"))                         items_.push_back({this, Touch,    "Touch"});
    if (caps.has("audio.input") || caps.has("audio.output"))
                                                          items_.push_back({this, Sounds,   "Sounds"});
    if (caps.has("camera"))                              items_.push_back({this, Camera,   "Camera"});
    items_.push_back({this, About,    "About"});

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 3; root.gap = 1;
    Style line; line.height = 1; line.background = true;
    Style sv;   sv.dir = FlexDir::Col; sv.align = Align::Stretch; sv.gap = 1;

    UiNode* list = ScrollView(a, scroll_, sv, {});
    UiNode* prev = nullptr;
    for (auto& it : items_) {
        UiNode* row = ListRow(a, it.label, onSelect, &it);
        if (!row) break;
        if (!prev) list->firstChild = row; else prev->nextSibling = row;
        prev = row;
    }

    return View(a, root, {
        Text(a, "SETTINGS", TextRole::Title),
        View(a, line, {}),
        list,
    });
}

} // namespace kairo
