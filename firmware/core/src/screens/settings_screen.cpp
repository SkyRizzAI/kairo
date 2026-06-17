// Plan 60 — SettingsScreen: ListView (themed rows + accessory + dashed scrollbar).
#include "nema/system/capabilities.h"
#include "nema/screens/settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/style_tokens.h"
#include "nema/system/capability_registry.h"

namespace nema {

using namespace ui;

SettingsScreen::SettingsScreen(Runtime& rt)
    : ComponentScreen(rt, 160), about_(rt), sleepSettings_(rt), controls_(rt),
      touchSettings_(rt), sounds_(rt), cameraSettings_(rt), developer_(rt),
      profileSettings_(rt) {}

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
        case Camera:     rt_.view().push(cameraSettings_);    break;
        case Developer:  rt_.view().push(developer_);        break;
        case Profile:    rt_.view().push(profileSettings_);   break;
    }
}

UiNode* SettingsScreen::build(NodeArena& a, Runtime& rt) {
    items_.clear();
    auto& caps = rt.capabilities();
    items_.push_back({this, Display,  "Display"});
    items_.push_back({this, Controls, "Controls"});
    if (caps.has(caps::InputTouch))
        items_.push_back({this, Touch,    "Touch"});
    if (caps.has(caps::AudioInput) || caps.has(caps::AudioOutput))
        items_.push_back({this, Sounds,   "Sounds"});
    if (caps.has(caps::Camera))
        items_.push_back({this, Camera,   "Camera"});
    if (caps.has(caps::Profile))
        items_.push_back({this, Profile,  "Profile"});
    items_.push_back({this, Developer, "Developer"});
    items_.push_back({this, About,    "About"});

    uint8_t pad = nema::theme().space.sm;
    uint8_t gap = nema::theme().space.xs;
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = pad; root.gap = gap;
    root.align = Align::Stretch;
    Style sv;   sv.dir = FlexDir::Col; sv.align = Align::Stretch; sv.gap = 1;

    UiNode* list = ScrollView(a, scroll_, sv, {});
    UiNode* prev = nullptr;
    for (auto& it : items_) {
        UiNode* row = ListItem(a, it.label, ">", onSelect, &it);
        if (!row) break;
        if (!prev) list->firstChild = row; else prev->nextSibling = row;
        prev = row;
    }

    return View(a, root, {
        TitleBar(a, "SETTINGS"),
        list,
    });
}

} // namespace nema
