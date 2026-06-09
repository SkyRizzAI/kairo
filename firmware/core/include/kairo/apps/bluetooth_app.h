#pragma once
#include "kairo/app/component_app.h"
#include "kairo/hal/bluetooth.h"
#include <atomic>
#include <vector>

namespace kairo {

// BluetoothApp — Settings → Bluetooth (Plan 34). ComponentApp state machine:
// toggle radio, advertise (discoverable), confirm pairing (numeric comparison
// modal), list/forget bonded devices. Capability-gated launch from Settings.
class BluetoothApp : public ComponentApp {
public:
    const char* id()   const override { return "com.kairo.bluetooth"; }
    const char* name() const override { return "Bluetooth"; }

protected:
    void        onStart(AppContext& ctx) override;
    ui::UiNode* build(ui::NodeArena& a, AppContext& ctx) override;
    ui::UiNode* buildModal(ui::NodeArena& a, AppContext& ctx) override;
    uint32_t    tickIntervalMs() const override { return 200; }
    bool        onTick(AppContext& ctx) override;

private:
    struct PeerItem { BluetoothApp* self; int idx; };

    static void onToggleBt(void* u);
    static void onDiscoverable(void* u);
    static void onShowPaired(void* u);
    static void onForgetAll(void* u);
    static void onForgetRow(void* u);
    static void onPairYes(void* u);
    static void onPairNo(void* u);
    static void onPairReq(void* user, const BlePairRequest& req);

    IBluetoothController* ctrl_ = nullptr;
    IBleAdapter*         ble_  = nullptr;

    bool                  showPaired_ = false;
    std::atomic<bool>     pairReq_{false};
    std::atomic<uint32_t> passkey_{0};
    bool                  lastConn_    = false;
    bool                  lastPairReq_ = false;

    char status_[64]   = {};
    char passBuf_[16]  = {};
    std::vector<PeerItem> peerItems_;
    char peerLabels_[8][48] = {};   // name(32) + "  [forget]" + NUL
};

} // namespace kairo
