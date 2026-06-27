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
    redraw();
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

#define S(u) static_cast<RemoteSettingsScreen*>(u)

UiNode* RemoteSettingsScreen::build(NodeArena& a, Runtime& rt) {
    (void)rt;
    RemoteAuthStore* st = store();

    MenuBuilder m(a, scroll_, this);

    if (!st) {
        ListEntry e; e.label = "Remote unavailable";
        m.add(ListItemRow(a, e));
        return m.build();
    }

    bool en = st->enabled();
    bool pw = st->hasPassword();

    m.section("Connection");
    m.toggle("Remote Enabled", en, [](void* u){
        if (RemoteAuthStore* st = S(u)->store()) st->setEnabled(!st->enabled());
        S(u)->rt_.events().publish({events::RemoteToggled, {}});
        S(u)->redraw();
    });

    m.section("Security");
    m.info("Password", pw ? "Set" : "None");
    m.nav(pw ? "Change Password" : "Set Password", [](void* u){
        auto* s = S(u);
        s->kbd_.clear();
        s->kbd_.setPassword(true);
        s->kbd_.linear = !s->rt_.capabilities().has(caps::Input2D);
        s->swallowCode_ = true;
        s->st_ = St::EnterPass;
        s->redraw();
    });
    if (pw) m.nav("Clear Password", [](void* u){
        if (RemoteAuthStore* st = S(u)->store()) st->clearPassword();
        S(u)->redraw();
    });

    m.section("Sessions");
    {
        int n = (int)st->tokenCount();
        std::snprintf(statusBuf_, sizeof(statusBuf_), "%d", n);
        m.info("Authorized Devices", statusBuf_);
    }
    if (st->tokenCount() > 0) m.nav("Log Out All Devices", [](void* u){
        if (RemoteAuthStore* st = S(u)->store()) st->revokeAllTokens();
        S(u)->redraw();
    });

    return m.build();
}

#undef S

} // namespace nema
