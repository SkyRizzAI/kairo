#pragma once
#include "nema/hal/radio_wifi.h"
#include "nema/service.h"
#include "nema/thread.h"
#include <mutex>
#include <atomic>
#include <vector>
#include <string>

namespace nema {

class Runtime;

// SimWifiRadio — WASM/sim implementation of IRadioWifi. Generic primitives only.
//
// scan()        — delegates to the live IWifiDriver, synthesises fake BSSIDs.
// monitorOpen() — simulated promiscuous mode: the loop thread generates fake
//                 802.11 beacon frames and pushes them via pushFrame().
// inject()      — logs a "frame_injected" event (no real radio in sim).
// Attacks (deauth/beacon/probe/karma) are app logic, not radio methods (Plan 91).
class SimWifiRadio : public IRadioWifi, public IService {
public:
    void init(Runtime& rt);

    // ── IRadioWifi (generic primitives) ───────────────────────────────────────
    std::vector<RadioScanResult> scan() override;
    bool monitorOpen(uint8_t ch)  override;
    void monitorClose()           override;
    bool inject(uint8_t ch, const uint8_t* frame, size_t len) override;

    // ── IService ──────────────────────────────────────────────────────────────
    const char*  name() const override { return "SimWifiRadio"; }
    DriverKind   kind() const override { return DriverKind::Wifi; }
    void start() override {}
    void stop()  override;

private:
    static void loopEntry(void* self);
    void        loopFn();

    Runtime*  rt_          = nullptr;
    Thread    loopThread_;
    std::mutex               mu_;
    std::atomic<bool>        doMonitor_{false};
    uint8_t                  monitorChannel_ = 1;
};

} // namespace nema
