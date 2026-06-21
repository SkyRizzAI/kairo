#include "nema/screens/remote_settings_screen.h"
#include "nema/services/remote_auth.h"
#include "nema/runtime.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include "nema/service/service_container.h"
#include "nema/system/capabilities.h"
#include "nema/system/capability_registry.h"
#include "nema/input/input_code.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/canvas.h"
#include <cstdio>
#include <cstring>

namespace nema {

using namespace aether::ui;

RemoteSettingsScreen::RemoteSettingsScreen(Runtime& rt) : ComponentScreen(rt, 192) {}

RemoteAuthStore* RemoteSettingsScreen::store() {
    if (!store_) store_ = rt_.container().resolve<RemoteAuthStore>();
    return store_;
}

void RemoteSettingsScreen::redraw() { dirty_ = true; requestRedraw(); }

void RemoteSettingsScreen::onResume() {
    st_ = St::List;
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    redraw();
}

void RemoteSettingsScreen::cbToggleEnabled(void* u) {
    auto* s = static_cast<RemoteSettingsScreen*>(u);
    if (RemoteAuthStore* a = s->store()) a->setEnabled(!a->enabled());
    s->rt_.events().publish({events::RemoteToggled, {}});
    s->redraw();
}
void RemoteSettingsScreen::cbSetPassword(void* u) {
    auto* s = static_cast<RemoteSettingsScreen*>(u);
    s->kbd_.clear();
    s->kbd_.setPassword(true);
    s->kbd_.linear = !s->rt_.capabilities().has(caps::Input2D);
    s->swallowCode_ = true;
    s->st_ = St::EnterPass;
    s->redraw();
}
void RemoteSettingsScreen::cbClearPassword(void* u) {
    auto* s = static_cast<RemoteSettingsScreen*>(u);
    if (RemoteAuthStore* a = s->store()) a->clearPassword();
    s->redraw();
}
void RemoteSettingsScreen::cbLogoutAll(void* u) {
    auto* s = static_cast<RemoteSettingsScreen*>(u);
    if (RemoteAuthStore* a = s->store()) a->revokeAllTokens();
    s->redraw();
}

void RemoteSettingsScreen::onAction(input::Action a) {
    if (st_ == St::EnterPass) {
        swallowCode_ = false;
        if (kbd_.linear) {
            bool done = false, cancel = false;
            kbd_.handleAction(a, done, cancel);
            if (cancel) st_ = St::List;
            else if (done) {
                if (RemoteAuthStore* st = store()) st->setPassword(std::string(kbd_.buf, kbd_.len));
                st_ = St::List;
            }
            redraw();
        }
        return;
    }
    ComponentScreen::onAction(a);
}

void RemoteSettingsScreen::onCode(input::Code c) {
    if (st_ != St::EnterPass || kbd_.linear) return;
    if (swallowCode_) { swallowCode_ = false; return; }
    bool done = false, cancel = false;
    kbd_.handle(input::keyFromCode(c), done, cancel);
    if (cancel) st_ = St::List;
    else if (done) {
        if (RemoteAuthStore* st = store()) st->setPassword(std::string(kbd_.buf, kbd_.len));
        st_ = St::List;
    }
    redraw();
}

void RemoteSettingsScreen::draw(Canvas& c) {
    if (st_ == St::EnterPass) { c.clear(); kbd_.draw(c, "Remote password"); return; }
    ComponentScreen::draw(c);
}

UiNode* RemoteSettingsScreen::build(NodeArena& a, Runtime& rt) {
    (void)rt;
    RemoteAuthStore* st = store();

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    UiNode* list = ListContainer(a, scroll_, {});
    UiNode* prev = nullptr;
    auto append = [&](UiNode* n) {
        if (!n) return;
        if (!prev) list->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };

    if (!st) {
        ListEntry e; e.label = "Remote unavailable";
        append(ListItemRow(a, e));
        return View(a, root, { TitleBar(a, "Remote"), list });
    }

    bool en = st->enabled();
    bool pw = st->hasPassword();

    append(ListSection(a, "Connection"));
    append(Toggle(a, "Remote Enabled", en, cbToggleEnabled, this));

    append(ListSection(a, "Security"));
    append(ListSection(a, pw ? "Password set — privileged channels locked"
                             : "No password — privileged channels open"));
    {
        ListEntry e;
        e.label   = pw ? "Change Password" : "Set Password";
        e.chevron = true;
        e.onPress = cbSetPassword; e.user = this;
        append(ListItemRow(a, e));
    }
    if (pw) {
        ListEntry e; e.label = "Clear Password"; e.chevron = true;
        e.onPress = cbClearPassword; e.user = this;
        append(ListItemRow(a, e));
    }

    std::snprintf(statusBuf_, sizeof(statusBuf_), "Authorized devices: %d", (int)st->tokenCount());
    append(ListSection(a, "Sessions"));
    {
        ListEntry e; e.label = statusBuf_;
        append(ListItemRow(a, e));
    }
    if (st->tokenCount() > 0) {
        ListEntry e; e.label = "Log Out All Devices"; e.chevron = true;
        e.onPress = cbLogoutAll; e.user = this;
        append(ListItemRow(a, e));
    }

    return View(a, root, { TitleBar(a, "Remote"), list });
}

} // namespace nema
