#include "nema/screens/wifi_network_detail_screen.h"
#include "nema/runtime.h"
#include "nema/service/service_container.h"
#include "nema/hal/wifi.h"
#include "nema/ui/view_dispatcher.h"
#include <cstdio>
#include <cstring>

namespace nema {

using namespace aether::ui;

WifiNetworkDetailScreen::WifiNetworkDetailScreen(Runtime& rt)
    : ComponentScreen(rt, 192), ipConfig_(rt) {}

IWifiDriver* WifiNetworkDetailScreen::drv() {
    if (!drv_) drv_ = rt_.container().resolve<IWifiDriver>();
    return drv_;
}

void WifiNetworkDetailScreen::setNetwork(const char* ssid, bool secured, bool current, bool saved) {
    ssid_    = ssid ? ssid : "";
    secured_ = secured;
    current_ = current;
    saved_   = saved;
}

void WifiNetworkDetailScreen::onResume() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    dirty_ = true;
    requestRedraw();
}

void WifiNetworkDetailScreen::cbForget(void* u) {
    auto* s = static_cast<WifiNetworkDetailScreen*>(u);
    if (IWifiDriver* d = s->drv()) {
        d->forgetNetwork(s->ssid_.c_str());
        if (s->current_) d->disconnect();
    }
    s->rt_.view().goBack();
}
void WifiNetworkDetailScreen::cbJoin(void* u) {
    auto* s = static_cast<WifiNetworkDetailScreen*>(u);
    if (IWifiDriver* d = s->drv()) d->connectSaved(s->ssid_.c_str());
    s->rt_.view().goBack();
}
void WifiNetworkDetailScreen::cbConfigureIp(void* u) {
    auto* s = static_cast<WifiNetworkDetailScreen*>(u);
    s->rt_.view().navigate(s->ipConfig_);
}
bool WifiNetworkDetailScreen::autoJoinOf(const char* ssid) {
    IWifiDriver* d = drv();
    if (!d) return true;
    for (size_t i = 0; i < d->savedCount(); i++) {
        WifiProfile p;
        if (d->savedAt(i, p) && std::strcmp(p.ssid, ssid) == 0) return p.autoJoin;
    }
    return true;
}
void WifiNetworkDetailScreen::cbToggleAutoJoin(void* u) {
    auto* s = static_cast<WifiNetworkDetailScreen*>(u);
    if (IWifiDriver* d = s->drv())
        d->setAutoJoin(s->ssid_.c_str(), !s->autoJoinOf(s->ssid_.c_str()));
    s->dirty_ = true;
    s->requestRedraw();
}

UiNode* WifiNetworkDetailScreen::build(NodeArena& a, Runtime& rt) {
    (void)rt;
    IWifiDriver* d = drv();

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 3; root.gap = 1;
    root.align = Align::Stretch;
    Style sv;   sv.dir = FlexDir::Col; sv.align = Align::Stretch; sv.gap = 1;

    UiNode* list = ScrollView(a, scroll_, sv, {});
    UiNode* prev = nullptr;
    auto append = [&](UiNode* n) {
        if (!n) return;
        if (!prev) list->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };

    if (current_)
        append(Text(a, (d && d->isOnline()) ? "Connected"
                                             : "Connected (no internet)", TextRole::Caption));
    if (saved_ && !current_)
        append(ListItem(a, "Join This Network", ">", cbJoin, this));
    if (current_ || saved_) {
        append(Toggle(a, "Auto-Join", autoJoinOf(ssid_.c_str()), cbToggleAutoJoin, this));
        append(ListItem(a, "Forget This Network", ">", cbForget, this));
    }

    if (current_ && d) {
        WifiIpConfig c = d->ipConfig();
        append(Text(a, "IP ADDRESS", TextRole::Caption));
        append(ListItem(a, "Configure IP", c.dhcp ? "Automatic" : "Manual", cbConfigureIp, this));
        std::snprintf(rowbuf_[0], sizeof(rowbuf_[0]), "%s", c.ip[0]   ? c.ip   : "-");
        std::snprintf(rowbuf_[1], sizeof(rowbuf_[1]), "%s", c.mask[0] ? c.mask : "-");
        std::snprintf(rowbuf_[2], sizeof(rowbuf_[2]), "%s", c.gw[0]   ? c.gw   : "-");
        std::snprintf(rowbuf_[3], sizeof(rowbuf_[3]), "%s", c.dns[0]  ? c.dns  : "-");
        append(ListItem(a, "IP Address",  rowbuf_[0], nullptr, nullptr));
        append(ListItem(a, "Subnet Mask", rowbuf_[1], nullptr, nullptr));
        append(ListItem(a, "Router",      rowbuf_[2], nullptr, nullptr));
        append(ListItem(a, "DNS",         rowbuf_[3], nullptr, nullptr));
    }

    return View(a, root, { TitleBar(a, ssid_.c_str()), list });
}

} // namespace nema
