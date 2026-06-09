#include "kairo/apps/bluetooth_app.h"
#include "kairo/app/app_context.h"
#include "kairo/runtime.h"
#include "kairo/service/service_container.h"
#include <cstdio>

namespace kairo {

using namespace ui;

void BluetoothApp::onStart(AppContext& ctx) {
    ctrl_ = ctx.runtime().container().resolve<IBluetoothController>();
    ble_  = ctx.runtime().container().resolve<IBleAdapter>();
    if (ble_) ble_->onPairRequest(&BluetoothApp::onPairReq, this);
}

void BluetoothApp::onPairReq(void* user, const BlePairRequest& req) {
    auto* s = static_cast<BluetoothApp*>(user);
    s->passkey_.store(req.passkey);
    s->pairReq_.store(true);   // app thread picks this up in onTick → modal
}

bool BluetoothApp::onTick(AppContext&) {
    bool dirty = false;
    bool pr = pairReq_.load();
    if (pr != lastPairReq_) { lastPairReq_ = pr; dirty = true; }
    bool c = ble_ && ble_->isConnected();
    if (c != lastConn_) { lastConn_ = c; dirty = true; }
    return dirty;
}

void BluetoothApp::onToggleBt(void* u) {
    auto* s = static_cast<BluetoothApp*>(u);
    if (!s->ctrl_) return;
    if (s->ctrl_->isEnabled()) {
        if (s->ble_) s->ble_->stopAdvertising();
        s->ctrl_->disable();
    } else {
        // NOTE: on ESP32 this init is heavy → run via TaskRunner worker (Plan 34
        // fase 4). In the simulator it is instant, so call directly.
        s->ctrl_->enable(BtMode::Ble);
        if (s->ble_) s->ble_->startAdvertising();
    }
}

void BluetoothApp::onDiscoverable(void* u) {
    auto* s = static_cast<BluetoothApp*>(u);
    if (s->ble_) s->ble_->startAdvertising();
}

void BluetoothApp::onShowPaired(void* u) {
    auto* s = static_cast<BluetoothApp*>(u);
    s->showPaired_ = !s->showPaired_;
}

void BluetoothApp::onForgetAll(void* u) {
    auto* s = static_cast<BluetoothApp*>(u);
    if (s->ble_) s->ble_->forgetAll();
}

void BluetoothApp::onForgetRow(void* u) {
    auto* it = static_cast<PeerItem*>(u);
    auto* s = it->self;
    BtPeer p;
    if (s->ble_ && s->ble_->bondedAt((size_t)it->idx, p)) s->ble_->forget(p.addr);
}

void BluetoothApp::onPairYes(void* u) {
    auto* s = static_cast<BluetoothApp*>(u);
    if (s->ble_) s->ble_->confirmPairing(true);
    s->pairReq_.store(false);
    s->lastPairReq_ = false;
}

void BluetoothApp::onPairNo(void* u) {
    auto* s = static_cast<BluetoothApp*>(u);
    if (s->ble_) s->ble_->confirmPairing(false);
    s->pairReq_.store(false);
    s->lastPairReq_ = false;
}

UiNode* BluetoothApp::buildModal(NodeArena& a, AppContext&) {
    if (!pairReq_.load()) return nullptr;
    std::snprintf(passBuf_, sizeof(passBuf_), "%06u", (unsigned)passkey_.load());
    Style row; row.dir = FlexDir::Row; row.gap = 8; row.align = Align::Center; row.justify = Justify::Center;
    return Modal(a, {
        Text(a, "Pair this device?", TextRole::Body),
        Text(a, passBuf_, TextRole::Title),
        Row(a, row, {
            Button(a, "Pair",   onPairYes, this),
            Button(a, "Cancel", onPairNo,  this),
        }),
    });
}

UiNode* BluetoothApp::build(NodeArena& a, AppContext&) {
    const bool enabled = ctrl_ && ctrl_->isEnabled();

    if (!enabled) std::snprintf(status_, sizeof(status_), "Status: Off");
    else if (ble_ && ble_->isConnected()) {
        BtPeer p{}; ble_->peer(p);
        std::snprintf(status_, sizeof(status_), "Connected: %s", p.name);
    } else if (ble_ && ble_->isAdvertising())
        std::snprintf(status_, sizeof(status_), "Discoverable as %s",
                      ctrl_ ? ctrl_->deviceName() : "Kairo");
    else std::snprintf(status_, sizeof(status_), "Status: Idle");

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 3; root.gap = 2;
    root.align = Align::Stretch;

    UiNode* rootN = View(a, root, {});
    UiNode* prev = nullptr;
    auto add = [&](UiNode* n) {
        if (!n) return;
        if (!prev) rootN->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };

    add(Header(a, "Bluetooth"));
    add(Toggle(a, "Bluetooth", enabled, onToggleBt, this));
    add(Text(a, status_, TextRole::Caption));

    if (enabled) {
        add(Button(a, "Discoverable", onDiscoverable, this));
        add(Button(a, showPaired_ ? "Hide Paired" : "Paired Devices", onShowPaired, this));
        if (showPaired_) {
            size_t n = ble_ ? ble_->bondedCount() : 0;
            peerItems_.clear();
            if (n == 0) add(Text(a, "(no paired devices)", TextRole::Caption));
            for (size_t i = 0; i < n && i < 8; i++) {
                BtPeer p{}; ble_->bondedAt(i, p);
                std::snprintf(peerLabels_[i], sizeof(peerLabels_[i]), "%s  [forget]", p.name);
                peerItems_.push_back({this, (int)i});
            }
            for (size_t i = 0; i < peerItems_.size(); i++)
                add(ListRow(a, peerLabels_[i], onForgetRow, &peerItems_[i]));
            if (n > 0) add(Button(a, "Forget All", onForgetAll, this));
        }
    }

    return rootN;
}

} // namespace kairo
