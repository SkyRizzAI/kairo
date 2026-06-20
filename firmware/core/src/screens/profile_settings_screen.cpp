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
    : ComponentScreen(rt, 160) {}

void ProfileSettingsScreen::onResume() {
    editing_ = false;
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    rt_.view().requestRedraw();
}

void ProfileSettingsScreen::startEdit(Field f) {
    auto* p = rt_.container().resolve<ProfileService>();
    if (!p) return;
    editField_ = f;

    if (f == Field::ClearPassword) {
        p->clearPassword();
        rt_.view().requestRedraw();
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
    editing_ = true;
    rt_.view().requestRedraw();
}

void ProfileSettingsScreen::onSelect(void* u) {
    auto* it = static_cast<Item*>(u);
    it->self->startEdit(it->field);
}

void ProfileSettingsScreen::onAction(input::Action a) {
    if (!editing_) { ComponentScreen::onAction(a); return; }

    bool done = false, cancel = false;
    if (rt_.capabilities().has(caps::Input2D))
        kbd_.handle(input::keyFromAction(a), done, cancel);
    else
        kbd_.handleAction(a, done, cancel);

    if (cancel) {
        editing_ = false;
        rt_.view().requestRedraw();
        return;
    }
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
        rt_.view().requestRedraw();
        return;
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

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 3; root.gap = 1;
    root.align = Align::Stretch;
    Style sv;   sv.dir = FlexDir::Col; sv.gap = 1;

    if (!p) {
        return View(a, root, {
            TitleBar(a, "PROFILE"),
            Text(a, "not available", TextRole::Body),
        });
    }

    auto add = [&](Field f, const char* label, const char* detail) {
        Item it; it.self = this; it.field = f; it.label = label;
        std::snprintf(it.detail, sizeof(it.detail), "%s", detail ? detail : "");
        items_.push_back(it);
    };
    add(Field::UserName,    "User name",   p->userName().c_str());
    add(Field::DeviceName,  "Device name", p->deviceName().c_str());
    add(Field::SetPassword, "Password",    p->hasPassword() ? "* * * *" : "(not set)");
    if (p->hasPassword())
        add(Field::ClearPassword, "Clear password", nullptr);

    UiNode* list = ScrollView(a, scroll_, sv, {});
    UiNode* prev = nullptr;
    for (auto& it : items_) {
        bool isClear = (it.field == Field::ClearPassword);
        UiNode* row = isClear
            ? ListRow(a, it.label, onSelect, &it)
            : TextField(a, it.label, it.detail, onSelect, &it);
        if (!row) break;
        if (!prev) list->firstChild = row; else prev->nextSibling = row;
        prev = row;
    }

    return View(a, root, {
        TitleBar(a, "PROFILE"),
        list,
    });
}

} // namespace nema
