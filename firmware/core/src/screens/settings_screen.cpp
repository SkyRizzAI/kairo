// Plan 60 — SettingsScreen: ListView (themed rows + accessory + dashed scrollbar).
#include "nema/system/capabilities.h"
#include "nema/screens/settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/system/capability_registry.h"

namespace nema {

using namespace aether::ui;

SettingsScreen::SettingsScreen(Runtime& rt)
    : ComponentScreen(rt, 160), about_(rt), sleepSettings_(rt), appearances_(rt), wifiSettings_(rt),
      btSettings_(rt), remoteSettings_(rt), controls_(rt), touchSettings_(rt), sounds_(rt),
      cameraSettings_(rt), ledSettings_(rt), sensorsSettings_(rt), developer_(rt),
      profileSettings_(rt), storageSettings_(rt), appDetail_(rt), appsSettings_(rt)
{
    appsSettings_.setDetailScreen(&appDetail_);
}

void SettingsScreen::onResume() {
    rt_.view().requestRedraw();
}

void SettingsScreen::onSelect(void* u) {
    auto* it = static_cast<Item*>(u);
    it->self->launch(it->kind);
}

void SettingsScreen::launch(Kind k) {
    switch (k) {
        case About:      rt_.view().navigate(about_);          break;
        case Display:     rt_.view().navigate(sleepSettings_); break;
        case Appearances: rt_.view().navigate(appearances_);  break;
        case Wifi:       rt_.view().navigate(wifiSettings_);   break;
        case Bluetooth:  rt_.view().navigate(btSettings_);     break;
        case Remote:     rt_.view().navigate(remoteSettings_); break;
        case Controls:   rt_.view().navigate(controls_);       break;
        case Touch:      rt_.view().navigate(touchSettings_);  break;
        case Sounds:     rt_.view().navigate(sounds_);         break;
        case Camera:     rt_.view().navigate(cameraSettings_);    break;
        case Led:        rt_.view().navigate(ledSettings_);       break;
        case Sensors:    rt_.view().navigate(sensorsSettings_);   break;
        case Developer:  rt_.view().navigate(developer_);        break;
        case Profile:    rt_.view().navigate(profileSettings_);   break;
        case Storage:    rt_.view().navigate(storageSettings_);  break;
        case Apps:       rt_.view().navigate(appsSettings_);     break;
    }
}

UiNode* SettingsScreen::build(NodeArena& a, Runtime& rt) {
    items_.clear();
    auto& caps = rt.capabilities();
    items_.push_back({this, Display,     "Display"});
    items_.push_back({this, Appearances, "Appearances"});
    items_.push_back({this, Controls, "Controls"});
    if (caps.has(caps::NetWifi))
        items_.push_back({this, Wifi,      "Wi-Fi"});
    if (caps.has(caps::BtBle))
        items_.push_back({this, Bluetooth, "Bluetooth"});
    items_.push_back({this, Remote,    "Remote"});
    if (caps.has(caps::InputTouch))
        items_.push_back({this, Touch,    "Touch"});
    if (caps.has(caps::AudioInput) || caps.has(caps::AudioOutput))
        items_.push_back({this, Sounds,   "Sounds"});
    if (caps.has(caps::Camera))
        items_.push_back({this, Camera,   "Camera"});
    if (caps.has(caps::Led) || caps.has(caps::Rgb))
        items_.push_back({this, Led,      "LEDs"});
    if (caps.has(caps::SensorsEnv) || caps.has(caps::SensorsLight) || caps.has(caps::SensorsMotion))
        items_.push_back({this, Sensors,  "Sensors"});
    if (caps.has(caps::Profile))
        items_.push_back({this, Profile,  "Profile"});
    items_.push_back({this, Apps,      "Apps"});
    items_.push_back({this, Storage,   "Storage"});
    items_.push_back({this, Developer, "Developer"});
    items_.push_back({this, About,     "About"});

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    UiNode* list = ListContainer(a, scroll_, {});
    UiNode* prev = nullptr;
    for (auto& it : items_) {
        ListEntry e;
        e.label   = it.label;
        e.chevron = true;
        e.onPress = onSelect;
        e.user    = &it;
        UiNode* row = ListItemRow(a, e);
        if (!row) break;
        if (!prev) list->firstChild = row; else prev->nextSibling = row;
        prev = row;
    }

    return View(a, root, { list });
}

} // namespace nema
