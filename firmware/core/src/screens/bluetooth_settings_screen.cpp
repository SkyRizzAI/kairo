#include "nema/screens/bluetooth_settings_screen.h"
#include "nema/runtime.h"
#include "nema/service/service_container.h"
#include "nema/task_runner.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include "nema/system/capabilities.h"
#include "nema/system/capability_registry.h"
#include <cstdio>
#include <cstring>

namespace nema {

using namespace aether::ui;

BluetoothSettingsScreen::BluetoothSettingsScreen(Runtime& rt) : ComponentScreen(rt, 256), confirm_(rt) {
    rt_.events().subscribe(events::BtPairRequest, [this](const Event& e) {
        passkey_.clear();
        for (auto& f : e.payload) if (std::strcmp(f.key, "passkey") == 0) passkey_ = f.value;
        pendingPair_ = true;
        redraw();
    });
    auto rd = [this](const Event&) { redraw(); };
    rt_.events().subscribe(events::BtPaired,       [this](const Event&){ pendingPair_ = false; redraw(); });
    rt_.events().subscribe(events::BtEnabled,      rd);
    rt_.events().subscribe(events::BtDisabled,     rd);
    rt_.events().subscribe(events::BtConnected,    rd);
    rt_.events().subscribe(events::BtDisconnected, rd);
}

void BluetoothSettingsScreen::redraw() { dirty_ = true; requestRedraw(); }

IBluetoothController* BluetoothSettingsScreen::ctrl() {
    if (!ctrl_) ctrl_ = rt_.container().resolve<IBluetoothController>();
    return ctrl_;
}
IBleAdapter* BluetoothSettingsScreen::ble() {
    if (!ble_) ble_ = rt_.container().resolve<IBleAdapter>();
    return ble_;
}

void BluetoothSettingsScreen::onResume() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    redraw();
}

void BluetoothSettingsScreen::setEnabled(bool on) {
    IBluetoothController* c = ctrl();
    if (!c) return;
    IBleAdapter* b = on ? ble() : nullptr;
    runBusy(on ? "Turning on…" : "Turning off…",
            [c, b, on] { if (on) { if (c->enable(BtMode::Ble) && b) b->startAdvertising(); }
                         else      c->disable(); },
            [this] { redraw(); });
}

void BluetoothSettingsScreen::cbForget(void* u) {
    auto* r = static_cast<BondRow*>(u);
    auto* s = r->self;
    std::memcpy(s->pendingAddr_, r->addr, 6);   // bonds_ may rebuild; copy the target addr
    std::snprintf(s->confirmBody_, sizeof(s->confirmBody_), "Forget \"%s\"?", r->name.c_str());
    s->confirm_.setup("Forget Device", s->confirmBody_, "Forget", doForget, s, /*danger=*/true);
    s->rt_.view().push(s->confirm_);
}
void BluetoothSettingsScreen::doForget(void* u) {
    auto* s = static_cast<BluetoothSettingsScreen*>(u);
    s->rt_.view().goBack();   // pop the modal
    IBleAdapter* b = s->ble();
    if (!b) return;
    uint8_t addr[6];
    std::memcpy(addr, s->pendingAddr_, 6);
    s->runBusy("Forgetting…",
               [b, addr] { b->forget(addr); },
               [s] { s->redraw(); });
}

#define S(u) static_cast<BluetoothSettingsScreen*>(u)

UiNode* BluetoothSettingsScreen::build(NodeArena& a, Runtime& rt) {
    bonds_.clear();
    IBluetoothController* c = ctrl();
    IBleAdapter*          b = ble();

    MenuBuilder m(a, scroll_, this);

    if (!c) {
        ListEntry e; e.label = "No Bluetooth driver";
        m.add(ListItemRow(a, e));
        return m.build();
    }
    if (!rt.capabilities().available(caps::BtBle) && !c->isEnabled()) {
        if (rt.capabilities().stateOf(caps::BtBle) == ResourceState::Fault) {
            ListEntry e; e.label = "Bluetooth unavailable";
            m.add(ListItemRow(a, e));
            return m.build();
        }
    }

    bool en = c->isEnabled();
    m.toggle("Bluetooth", en, [](void* u){
        S(u)->setEnabled(!(S(u)->ctrl() && S(u)->ctrl()->isEnabled()));
    });

    if (en && b && b->isAdvertising()) {
        std::snprintf(statusBuf_, sizeof(statusBuf_), "Advertising as %s", c->deviceName());
        ListEntry e; e.label = statusBuf_;
        m.add(ListItemRow(a, e));
    }

    if (pendingPair_ && b) {
        pairPrompt_ = "Pair? code " + passkey_;
        m.section("Pair Request");
        m.nav("Confirm", [](void* u){
            if (S(u)->ble()) S(u)->ble()->confirmPairing(true);
            S(u)->pendingPair_ = false; S(u)->redraw();
        });
        m.nav("Reject", [](void* u){
            if (S(u)->ble()) S(u)->ble()->confirmPairing(false);
            S(u)->pendingPair_ = false; S(u)->redraw();
        });
    }

    if (en && b) {
        m.toggle("Discoverable", b->isAdvertising(), [](void* u){
            auto* self = S(u);
            IBleAdapter* bl = self->ble();
            if (!bl) return;
            bool adv = bl->isAdvertising();
            self->runBusy("Working…",
                          [bl, adv] { if (adv) bl->stopAdvertising(); else bl->startAdvertising(); },
                          [self] { self->redraw(); });
        });

        size_t n = b->bondedCount();
        m.section("Paired Devices");
        if (n == 0) {
            m.info("No paired devices", nullptr);
        } else {
            for (size_t i = 0; i < n; i++) {
                BtPeer p;
                if (!b->bondedAt(i, p)) continue;
                BondRow row{this, {}, (p.name[0] ? p.name : "device")};
                std::memcpy(row.addr, p.addr, 6);
                bonds_.push_back(std::move(row));
            }
            for (auto& r : bonds_) {
                ListEntry e;
                e.label   = r.name.c_str();
                e.value   = "forget";
                e.onPress = cbForget; e.user = &r;
                m.add(ListItemRow(a, e));
            }
        }
    }

    return m.build();
}

#undef S

} // namespace nema
