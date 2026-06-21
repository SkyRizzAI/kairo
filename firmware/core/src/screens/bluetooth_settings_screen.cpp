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

BluetoothSettingsScreen::BluetoothSettingsScreen(Runtime& rt) : ComponentScreen(rt, 256) {
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
    if (!c || busy_) return;
    busy_ = true;
    if (on) {
        IBleAdapter* b = ble();
        rt_.tasks().submit([c, b] { if (c->enable(BtMode::Ble) && b) b->startAdvertising(); },
                           [this] { busy_ = false; redraw(); });
    } else {
        rt_.tasks().submit([c] { c->disable(); },
                           [this] { busy_ = false; redraw(); });
    }
    redraw();
}

void BluetoothSettingsScreen::cbToggleEnable(void* u) {
    auto* s = static_cast<BluetoothSettingsScreen*>(u);
    s->setEnabled(!(s->ctrl() && s->ctrl()->isEnabled()));
}
void BluetoothSettingsScreen::cbToggleAdv(void* u) {
    auto* s = static_cast<BluetoothSettingsScreen*>(u);
    IBleAdapter* b = s->ble();
    if (!b) return;
    if (b->isAdvertising()) b->stopAdvertising(); else b->startAdvertising();
    s->redraw();
}
void BluetoothSettingsScreen::cbConfirmPair(void* u) {
    auto* s = static_cast<BluetoothSettingsScreen*>(u);
    if (s->ble()) s->ble()->confirmPairing(true);
    s->pendingPair_ = false; s->redraw();
}
void BluetoothSettingsScreen::cbRejectPair(void* u) {
    auto* s = static_cast<BluetoothSettingsScreen*>(u);
    if (s->ble()) s->ble()->confirmPairing(false);
    s->pendingPair_ = false; s->redraw();
}
void BluetoothSettingsScreen::cbForget(void* u) {
    auto* r = static_cast<BondRow*>(u);
    if (r->self->ble()) r->self->ble()->forget(r->addr);
    r->self->redraw();
}

UiNode* BluetoothSettingsScreen::build(NodeArena& a, Runtime& rt) {
    bonds_.clear();
    IBluetoothController* c = ctrl();
    IBleAdapter*          b = ble();

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    UiNode* list = ListContainer(a, scroll_, {});
    UiNode* prev = nullptr;
    auto append = [&](UiNode* n) {
        if (!n) return;
        if (!prev) list->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };

    if (!c) {
        ListEntry e; e.label = "No Bluetooth driver";
        append(ListItemRow(a, e));
        return View(a, root, { TitleBar(a, "Bluetooth"), list });
    }
    if (!rt.capabilities().available(caps::BtBle) && !c->isEnabled()) {
        if (rt.capabilities().stateOf(caps::BtBle) == ResourceState::Fault) {
            ListEntry e; e.label = "Bluetooth unavailable";
            append(ListItemRow(a, e));
            return View(a, root, { TitleBar(a, "Bluetooth"), list });
        }
    }

    bool en = c->isEnabled();
    if (busy_)
        std::snprintf(statusBuf_, sizeof(statusBuf_), "Working...");
    else if (!en)
        std::snprintf(statusBuf_, sizeof(statusBuf_), "Off");
    else if (b && b->isAdvertising())
        std::snprintf(statusBuf_, sizeof(statusBuf_), "Advertising as %s", c->deviceName());
    else
        std::snprintf(statusBuf_, sizeof(statusBuf_), "On");

    append(ListSection(a, statusBuf_));
    append(Toggle(a, "Bluetooth", en, cbToggleEnable, this));

    if (pendingPair_ && b) {
        pairPrompt_ = "Pair? code " + passkey_;
        append(ListSection(a, pairPrompt_.c_str()));
        ListEntry confirm; confirm.label = "Confirm"; confirm.chevron = true;
        confirm.onPress = cbConfirmPair; confirm.user = this;
        append(ListItemRow(a, confirm));
        ListEntry reject; reject.label = "Reject"; reject.chevron = true;
        reject.onPress = cbRejectPair; reject.user = this;
        append(ListItemRow(a, reject));
    }

    if (en && b) {
        append(Toggle(a, "Discoverable", b->isAdvertising(), cbToggleAdv, this));

        size_t n = b->bondedCount();
        if (n > 0) {
            append(ListSection(a, "Paired Devices"));
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
                append(ListItemRow(a, e));
            }
        }
    }

    return View(a, root, { TitleBar(a, "Bluetooth"), list });
}

} // namespace nema
