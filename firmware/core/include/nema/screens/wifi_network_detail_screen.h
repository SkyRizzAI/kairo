#pragma once
#include "nema/ui/component_screen.h"
#include "nema/screens/wifi_ip_config_screen.h"
#include "nema/screens/confirm_modal.h"
#include <string>

namespace nema {

class Runtime;
struct IWifiDriver;

// Settings -> Wi-Fi -> <network> (Plan 73). Per-network detail, iOS-style:
//   [ Join This Network ]        (saved, not current)
//   [ Forget This Network ]
//   IP ADDRESS
//     Configure IP   Automatic > (-> WifiIpConfigScreen)
//     IP Address / Subnet / Router / DNS   (live, current only)
class WifiNetworkDetailScreen : public ComponentScreen {
public:
    explicit WifiNetworkDetailScreen(Runtime& rt);

    // Called by the hub before navigating in.
    void setNetwork(const char* ssid, bool secured, bool current, bool saved);

    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    IWifiDriver* drv();

    bool autoJoinOf(const char* ssid);

    static void cbForget(void* u);   // shows the confirm modal
    static void doForget(void* u);   // runs after the user confirms
    static void cbJoin(void* u);
    static void cbToggleAutoJoin(void* u);
    static void cbConfigureIp(void* u);

    IWifiDriver*        drv_ = nullptr;
    WifiIpConfigScreen  ipConfig_;
    ConfirmModal        confirm_;
    std::string         ssid_;
    bool                secured_ = false;
    bool                current_ = false;
    bool                saved_   = false;
    aether::ui::ScrollState     scroll_;
    char                rowbuf_[5][40] = {};   // stable backing for IP info rows
    char                confirmBody_[64] = {};
};

} // namespace nema
