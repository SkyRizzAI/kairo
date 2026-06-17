#pragma once
#include "nema/app/component_app.h"
#include "nema/hal/wifi.h"
#include "nema/ui/virtual_keyboard.h"
#include <atomic>
#include <memory>
#include <vector>
#include <string>

namespace nema {

struct IWifiDriver;
class  IConfigStore;

// WifiApp — WiFi manager on the component system (Plan 27). Menu/list/status
// states use the component tree (Menu, focus nav); password entry uses the
// existing VirtualKeyboard via the raw-input escape hatch (capturesInput +
// drawRaw). Scan/connect run on the TaskRunner worker so the UI never freezes.
class WifiApp : public ComponentApp {
public:
    const char* id()   const override { return "com.palanu.wifi"; }
    const char* name() const override { return "WiFi"; }

protected:
    void        onStart(AppContext& ctx) override;
    ui::UiNode* build(ui::NodeArena& a, AppContext& ctx) override;
    bool        onKey(Key k, AppContext& ctx) override;
    bool        capturesInput() const override { return state_ == St::EnterPass; }
    bool        drawRaw(Canvas& c, AppContext& ctx) override;
    uint32_t    tickIntervalMs() const override {
        return (state_ == St::Scanning || state_ == St::Connecting) ? 100 : 0;
    }
    bool        onTick(AppContext& ctx) override;
    size_t      arenaCapacity() const override { return 128; }

private:
    enum class St { Overview, Scanning, Pick, EnterPass, Connecting, Result };

    struct PickCtx { WifiApp* self; int idx; };

    void startScan();
    void pickNetwork(int idx);
    void startConnect(const std::string& ssid, const std::string& pw);

    // Menu callbacks (userdata = this, or &pickCtx_[i] for the network list).
    static void cbScan(void* u);
    static void cbDisconnect(void* u);
    static void cbIpSettings(void* u);
    static void cbPickNet(void* u);

    St                       state_ = St::Overview;
    IWifiDriver*             drv_   = nullptr;
    IConfigStore*            cfg_   = nullptr;
    AppContext*              ctx_   = nullptr;   // stable for the app's lifetime
    std::vector<WifiNetwork> nets_;
    ui::VirtualKeyboard      pass_;
    std::string              pendingSsid_;
    bool                     ok_    = false;
    char                     statusBuf_[40] = "";
    char                     passPrompt_[48] = "";
    PickCtx                  pickCtx_[16];

    std::shared_ptr<std::atomic<bool>> scanDone_;
    std::shared_ptr<std::atomic<bool>> connectDone_;
};

} // namespace nema
