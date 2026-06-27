#include "nema/system/capabilities.h"
#include "nema/screens/profile_settings_screen.h"
#include "nema/services/profile_service.h"
#include "nema/runtime.h"
#include "nema/service/service_container.h"
#include "nema/system/capability_registry.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/input/input_action.h"
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace nema {

using namespace aether::ui;

ProfileSettingsScreen::ProfileSettingsScreen(Runtime& rt)
    : ComponentScreen(rt, 160), confirm_(rt) {}

void ProfileSettingsScreen::onResume() {
    editing_ = false;
    rt_.view().requestRedraw();
}

void ProfileSettingsScreen::startEdit(Field f) {
    auto* p = rt_.container().resolve<ProfileService>();
    if (!p) return;
    editField_ = f;

    if (f == Field::ClearPassword) {
        confirm_.setup("Clear Password", "Remove the device\npassword?", "Clear",
                       doClearPassword, this, /*danger=*/true);
        rt_.view().push(confirm_);
        return;
    }

    kbd_.clear();
    kbd_.linear = !rt_.capabilities().has(caps::Input2D);

    switch (f) {
    case Field::UserName:
        kbd_.setPassword(false);
        std::snprintf(prompt_, sizeof(prompt_), "User name:");
        std::strncpy(kbd_.buf, p->userName().c_str(), sizeof(kbd_.buf) - 1);
        kbd_.len = (uint8_t)std::min(p->userName().size(), sizeof(kbd_.buf) - 1);
        break;
    case Field::DeviceName:
        kbd_.setPassword(false);
        std::snprintf(prompt_, sizeof(prompt_), "Device name:");
        std::strncpy(kbd_.buf, p->deviceName().c_str(), sizeof(kbd_.buf) - 1);
        kbd_.len = (uint8_t)std::min(p->deviceName().size(), sizeof(kbd_.buf) - 1);
        break;
    case Field::SetPassword:
        kbd_.setPassword(true);
        std::snprintf(prompt_, sizeof(prompt_), "New password:");
        break;
    default:
        return;
    }
    editing_     = true;
    swallowCode_ = true;
    rt_.view().requestRedraw();
}

void ProfileSettingsScreen::onSelect(void* u) {
    auto* it = static_cast<Item*>(u);
    it->self->startEdit(it->field);
}

void ProfileSettingsScreen::doClearPassword(void* u) {
    auto* s = static_cast<ProfileSettingsScreen*>(u);
    s->rt_.view().goBack();   // pop the modal
    if (auto* p = s->rt_.container().resolve<ProfileService>())
        p->clearPassword();
    s->rt_.view().requestRedraw();   // NVS-only op; no busy spinner needed
}

void ProfileSettingsScreen::onAction(input::Action a) {
    if (!editing_) { ComponentScreen::onAction(a); return; }
    // 2D mode: raw Code arrives via onCode with correct geometric mapping.
    // Consuming the action here (without forwarding to ComponentScreen) prevents
    // the list from scrolling while the keyboard is open.
    if (!kbd_.linear) return;

    bool done = false, cancel = false;
    kbd_.handleAction(a, done, cancel);
    applyKbdResult(done, cancel);
}

void ProfileSettingsScreen::onCode(input::Code c) {
    if (!editing_ || kbd_.linear) return;
    if (swallowCode_) { swallowCode_ = false; return; }
    bool done = false, cancel = false;
    kbd_.handle(input::keyFromCode(c), done, cancel);
    applyKbdResult(done, cancel);
}

void ProfileSettingsScreen::applyKbdResult(bool done, bool cancel) {
    if (cancel) { editing_ = false; rt_.view().requestRedraw(); return; }
    if (done) {
        auto* p = rt_.container().resolve<ProfileService>();
        if (p) {
            std::string val(kbd_.buf, (size_t)kbd_.len);
            switch (editField_) {
            case Field::UserName:    if (!val.empty()) p->setUserName(val);   break;
            case Field::DeviceName:  if (!val.empty()) p->setDeviceName(val); break;
            case Field::SetPassword: if (!val.empty()) p->setPassword(val);   break;
            default: break;
            }
        }
        editing_ = false;
    }
    rt_.view().requestRedraw();
}

void ProfileSettingsScreen::draw(Canvas& c) {
    if (editing_) { kbd_.draw(c, prompt_); return; }
    ComponentScreen::draw(c);
}

aether::ui::UiNode* ProfileSettingsScreen::build(NodeArena& a, Runtime& rt) {
    items_.clear();

    auto* p = rt.container().resolve<ProfileService>();

    MenuBuilder m(a, scroll_, this);

    if (!p) {
        m.info("Not available", nullptr);
        return m.build();
    }

    auto add = [&](Field f, const char* label, const char* detail) {
        Item it; it.self = this; it.field = f; it.label = label;
        std::snprintf(it.detail, sizeof(it.detail), "%s", detail ? detail : "");
        items_.push_back(it);
    };
    add(Field::UserName,    "User Name",   p->userName().c_str());
    add(Field::DeviceName,  "Device Name", p->deviceName().c_str());
    add(Field::SetPassword, "Password",    p->hasPassword() ? "* * * *" : "(not set)");
    if (p->hasPassword())
        add(Field::ClearPassword, "Clear Password", nullptr);

    m.section("Identity");
    // Each row carries per-item userdata (&it) for onSelect, which nav() can't express
    // (it fans out the single screen pointer), so build the rows via the add() escape hatch.
    for (auto& it : items_) {
        ListEntry e;
        e.label   = it.label;
        e.value   = it.detail[0] ? it.detail : nullptr;
        e.chevron = true;
        e.onPress = onSelect; e.user = &it;
        m.add(ListItemRow(a, e));
    }

    return m.build();
}

} // namespace nema
