#pragma once
#include "nema/ui/component_screen.h"
#include "nema/ui/virtual_keyboard.h"
#include "nema/screens/wifi_network_detail_screen.h"
#include "nema/hal/wifi.h"
#include <vector>
#include <string>
#include <atomic>

namespace nema {

class Runtime;

// Settings -> Wi-Fi (Plan 73, iOS-style hub). A clean list:
//   [Toggle] Wi-Fi
//   <current network>      -> Network Detail (IP / Forget)
//   MY NETWORKS  (saved)   -> Network Detail (Join / Forget)
//   OTHER NETWORKS (scan)  -> join (password keyboard if secured)
// Background auto-scan; no manual "Scan" button. Disconnect/Forget live in the
// per-network detail screen, not here.
class WifiSettingsScreen : public ComponentScreen {
public:
    explicit WifiSettingsScreen(Runtime& rt);

    void        onResume() override;
    void        onAction(input::Action a) override;
    void        onCode(input::Code c) override;
    void        draw(Canvas& c) override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    enum class St { List, EnterSsid, EnterPass };
    enum class Act { Detail, Join };   // what tapping a row does
    struct Row {
        WifiSettingsScreen* self;
        std::string ssid;
        std::string label;
        std::string acc;
        bool        secured;
        Act         act;
        bool        saved;
        bool        current;
    };

    IWifiDriver* drv();
    void redraw();
    void startScan();
    void startConnect(const std::string& ssid, const std::string& pw);
    void openDetail(const Row& r);
    void pick(const Row& r);

    static void cbPick(void* u);
    void startKeyboard(bool password, const char* prompt, bool swallow);
    void handleKbdResult(bool done, bool cancel);

    IWifiDriver*             drv_ = nullptr;
    WifiNetworkDetailScreen  detail_;
    St                       st_  = St::List;
    bool                     swallowCode_ = false;
    aether::ui::VirtualKeyboard      kbd_;
    std::string              pendingSsid_;
    char                     prompt_[48] = {};
    char                     suspendedBuf_[64] = {};  // holds banner text when radio is taken
    std::string              wifiSuspendedBy_;        // non-empty while an app holds the radio
    aether::ui::ScrollState          scroll_;
    std::vector<Row>         rows_;
    std::atomic<bool>        scanning_{false};
};

} // namespace nema
