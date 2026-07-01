#include "nema/system/capabilities.h"
#include "nema/screens/device_hardware_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/system/capability_registry.h"

namespace nema {

using namespace aether::ui;

DeviceHardwareScreen::DeviceHardwareScreen(Runtime& rt)
    : ComponentScreen(rt, 96), sounds_(rt), cameraSettings_(rt), touchSettings_(rt),
      ledSettings_(rt), sensorsSettings_(rt), batterySettings_(rt), secureSettings_(rt) {}

void DeviceHardwareScreen::onResume() {
    rt_.view().requestRedraw();
}

void DeviceHardwareScreen::onSelect(void* u) {
    auto* it = static_cast<Item*>(u);
    it->self->launch(it->kind);
}

void DeviceHardwareScreen::launch(Kind k) {
    switch (k) {
        case Sounds:  rt_.view().navigate(sounds_);          break;
        case Camera:  rt_.view().navigate(cameraSettings_);  break;
        case Touch:   rt_.view().navigate(touchSettings_);   break;
        case Led:     rt_.view().navigate(ledSettings_);     break;
        case Sensors: rt_.view().navigate(sensorsSettings_); break;
        case Battery: rt_.view().navigate(batterySettings_); break;
        case Secure:  rt_.view().navigate(secureSettings_);  break;
    }
}

UiNode* DeviceHardwareScreen::build(NodeArena& a, Runtime& rt) {
    items_.clear();
    auto& caps = rt.capabilities();

    if (caps.has(caps::AudioInput) || caps.has(caps::AudioOutput))
        items_.push_back({this, Sounds, "Sounds"});
    if (caps.has(caps::Camera))
        items_.push_back({this, Camera, "Camera"});
    if (caps.has(caps::InputTouch))
        items_.push_back({this, Touch, "Touch"});
    if (caps.has(caps::Led) || caps.has(caps::Rgb))
        items_.push_back({this, Led, "LEDs"});
    if (caps.has(caps::SensorsEnv) || caps.has(caps::SensorsLight) || caps.has(caps::SensorsMotion))
        items_.push_back({this, Sensors, "Sensors"});
    if (caps.has(caps::Battery))
        items_.push_back({this, Battery, "Battery"});
    if (caps.has(caps::Secure))
        items_.push_back({this, Secure, "Secure Element"});

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    UiNode* list = ListContainer(a, scroll_, {});
    // First row: a section header naming the screen (matches the Settings sub-screen
    // convention — no big TitleBar).
    UiNode* prev = nullptr;
    UiNode* header = ListSection(a, "Device & Hardware");
    if (header) { list->firstChild = header; prev = header; }

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
    if (items_.empty()) {
        ListEntry e; e.label = "No hardware modules";
        UiNode* row = ListItemRow(a, e);
        if (row) { if (!prev) list->firstChild = row; else prev->nextSibling = row; }
    }

    return View(a, root, { list });
}

} // namespace nema
