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
    : ComponentScreen(rt, 192), ipConfig_(rt), confirm_(rt) {}

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
    dirty_ = true;
    requestRedraw();
}

void WifiNetworkDetailScreen::cbForget(void* u) {
    auto* s = static_cast<WifiNetworkDetailScreen*>(u);
    std::snprintf(s->confirmBody_, sizeof(s->confirmBody_), "Forget \"%s\"?", s->ssid_.c_str());
    s->confirm_.setup("Forget Network", s->confirmBody_, "Forget", doForget, s, /*danger=*/true);
    s->rt_.view().push(s->confirm_);
}
void WifiNetworkDetailScreen::doForget(void* u) {
    auto* s = static_cast<WifiNetworkDetailScreen*>(u);
    s->rt_.view().goBack();   // pop the modal
    IWifiDriver* d = s->drv();
    if (!d) { s->rt_.view().goBack(); return; }
    std::string ssid = s->ssid_;
    bool current = s->current_;
    s->runBusy("Forgetting…",
               [d, ssid, current] { d->forgetNetwork(ssid.c_str()); if (current) d->disconnect(); },
               [s] { s->rt_.view().goBack(); });
}
void WifiNetworkDetailScreen::cbJoin(void* u) {
    auto* s = static_cast<WifiNetworkDetailScreen*>(u);
    IWifiDriver* d = s->drv();
    if (!d) { s->rt_.view().goBack(); return; }
    std::string ssid = s->ssid_;
    s->runBusy("Connecting…",
               [d, ssid] { d->connectSaved(ssid.c_str()); },
               [s] { s->rt_.view().goBack(); });
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

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    UiNode* list = ListContainer(a, scroll_, {});
    UiNode* prev = nullptr;
    auto append = [&](UiNode* n) {
        if (!n) return;
        if (!prev) list->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };

    if (current_) {
        ListEntry e; e.label = (d && d->isOnline()) ? "Connected" : "Connected (no internet)";
        append(ListSection(a, e.label));
    }
    if (saved_ && !current_) {
        ListEntry e; e.label = "Join This Network"; e.chevron = true;
        e.onPress = cbJoin; e.user = this;
        append(ListItemRow(a, e));
    }
    if (current_ || saved_) {
        append(SwitchRow(a, "Auto-Join", autoJoinOf(ssid_.c_str()), cbToggleAutoJoin, this));
        ListEntry e; e.label = "Forget This Network"; e.chevron = true;
        e.onPress = cbForget; e.user = this;
        append(ListItemRow(a, e));
    }

    if (current_ && d) {
        WifiIpConfig c = d->ipConfig();
        std::snprintf(rowbuf_[0], sizeof(rowbuf_[0]), "%s", c.ip[0]   ? c.ip   : "-");
        std::snprintf(rowbuf_[1], sizeof(rowbuf_[1]), "%s", c.mask[0] ? c.mask : "-");
        std::snprintf(rowbuf_[2], sizeof(rowbuf_[2]), "%s", c.gw[0]   ? c.gw   : "-");
        std::snprintf(rowbuf_[3], sizeof(rowbuf_[3]), "%s", c.dns[0]  ? c.dns  : "-");

        append(ListSection(a, "IP Address"));
        {
            ListEntry e; e.label = "Configure IP"; e.value = c.dhcp ? "Automatic" : "Manual";
            e.chevron = true; e.onPress = cbConfigureIp; e.user = this;
            append(ListItemRow(a, e));
        }
        auto ipRow = [&](const char* label, const char* buf) {
            ListEntry e; e.label = label; e.value = buf;
            append(ListItemRow(a, e));
        };
        ipRow("IP Address",  rowbuf_[0]);
        ipRow("Subnet Mask", rowbuf_[1]);
        ipRow("Router",      rowbuf_[2]);
        ipRow("DNS",         rowbuf_[3]);
    }

    return View(a, root, { list });
}

} // namespace nema
