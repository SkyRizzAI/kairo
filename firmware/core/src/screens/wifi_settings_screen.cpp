#include "nema/screens/wifi_settings_screen.h"
#include "nema/runtime.h"
#include "nema/service/service_container.h"
#include "nema/task_runner.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include "nema/system/capabilities.h"
#include "nema/system/capability_registry.h"
#include "nema/input/input_code.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/canvas.h"
#include <cstdio>
#include <cstring>

namespace nema {

using namespace aether::ui;

WifiSettingsScreen::WifiSettingsScreen(Runtime& rt)
    : ComponentScreen(rt, 320), detail_(rt) {
    auto rd = [this](const Event&) { redraw(); };
    rt_.events().subscribe(events::WifiStateChanged,    rd);
    rt_.events().subscribe(events::WifiScanComplete,    rd);
    rt_.events().subscribe(events::NetworkConnected,    rd);
    rt_.events().subscribe(events::NetworkDisconnected, rd);
}

void WifiSettingsScreen::redraw() { dirty_ = true; requestRedraw(); }

IWifiDriver* WifiSettingsScreen::drv() {
    if (!drv_) drv_ = rt_.container().resolve<IWifiDriver>();
    return drv_;
}

void WifiSettingsScreen::onResume() {
    st_ = St::List;
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    if (IWifiDriver* d = drv(); d && d->isEnabled()) startScan();
    redraw();
}

void WifiSettingsScreen::startScan() {
    IWifiDriver* d = drv();
    if (!d || scanning_) return;
    scanning_ = true;
    rt_.tasks().submit([d] { d->scan(); },
                       [this] { scanning_ = false; redraw(); });
    redraw();
}

void WifiSettingsScreen::startConnect(const std::string& ssid, const std::string& pw) {
    IWifiDriver* d = drv();
    if (!d) return;
    std::string s = ssid, p = pw;
    rt_.tasks().submit([d, s, p] { d->connect(s.c_str(), p.c_str()); },
                       [this] { redraw(); });
    redraw();
}

void WifiSettingsScreen::openDetail(const Row& r) {
    detail_.setNetwork(r.ssid.c_str(), r.secured, r.current, r.saved);
    rt_.view().navigate(detail_);
}

void WifiSettingsScreen::startKeyboard(bool password, const char* prompt, bool swallow) {
    kbd_.clear();
    kbd_.setPassword(password);
    kbd_.linear  = !rt_.capabilities().has(caps::Input2D);
    swallowCode_ = swallow;
    std::snprintf(prompt_, sizeof(prompt_), "%s", prompt);
}

void WifiSettingsScreen::handleKbdResult(bool done, bool cancel) {
    if (cancel) { st_ = St::List; redraw(); return; }
    if (!done)  { redraw(); return; }
    if (st_ == St::EnterSsid) {
        pendingSsid_ = std::string(kbd_.buf, kbd_.len);
        if (pendingSsid_.empty()) { st_ = St::List; redraw(); return; }
        char p[48];
        std::snprintf(p, sizeof(p), "Password: %s", pendingSsid_.c_str());
        startKeyboard(true, p, false);   // chained entry — no list-select code to swallow
        st_ = St::EnterPass;
    } else {
        startConnect(pendingSsid_, std::string(kbd_.buf, kbd_.len));
        st_ = St::List;
    }
    redraw();
}

void WifiSettingsScreen::pick(const Row& r) {
    if (r.secured) {
        pendingSsid_ = r.ssid;
        char p[48];
        std::snprintf(p, sizeof(p), "Password: %s", r.ssid.c_str());
        startKeyboard(true, p, true);
        st_ = St::EnterPass;
    } else {
        startConnect(r.ssid, "");
    }
    redraw();
}

void WifiSettingsScreen::cbToggleWifi(void* u) {
    auto* s = static_cast<WifiSettingsScreen*>(u);
    IWifiDriver* d = s->drv();
    if (!d) return;
    bool on = !d->isEnabled();
    // setEnabled() may block (radio init + autoConnect's scan) — run on a worker
    // so the UI thread never freezes; scan too so the list populates.
    if (on) s->scanning_ = true;
    s->rt_.tasks().submit([d, on] { d->setEnabled(on); if (on) d->scan(); },
                          [s] { s->scanning_ = false; s->redraw(); });
    s->redraw();
}
void WifiSettingsScreen::cbPick(void* u) {
    auto* r = static_cast<Row*>(u);
    if (r->act == Act::Detail) r->self->openDetail(*r);
    else                       r->self->pick(*r);
}
void WifiSettingsScreen::cbAddOther(void* u) {
    auto* s = static_cast<WifiSettingsScreen*>(u);
    s->pendingSsid_.clear();
    s->startKeyboard(false, "Network Name", true);
    s->st_ = St::EnterSsid;
    s->redraw();
}

void WifiSettingsScreen::onAction(input::Action a) {
    if (st_ == St::EnterSsid || st_ == St::EnterPass) {
        swallowCode_ = false;
        if (kbd_.linear) {
            bool done = false, cancel = false;
            kbd_.handleAction(a, done, cancel);
            handleKbdResult(done, cancel);
        }
        return;
    }
    ComponentScreen::onAction(a);
}

void WifiSettingsScreen::onCode(input::Code c) {
    if ((st_ != St::EnterSsid && st_ != St::EnterPass) || kbd_.linear) return;
    if (swallowCode_) { swallowCode_ = false; return; }
    bool done = false, cancel = false;
    kbd_.handle(input::keyFromCode(c), done, cancel);
    handleKbdResult(done, cancel);
}

void WifiSettingsScreen::draw(Canvas& c) {
    if (st_ == St::EnterSsid || st_ == St::EnterPass) { c.clear(); kbd_.draw(c, prompt_); return; }
    ComponentScreen::draw(c);
}

static const char* bars(int8_t rssi) {
    if (rssi == 0)   return "";
    if (rssi >= -55) return "....||||";
    if (rssi >= -65) return "...|||";
    if (rssi >= -75) return "..||";
    return ".|";
}

UiNode* WifiSettingsScreen::build(NodeArena& a, Runtime& rt) {
    (void)rt;
    rows_.clear();
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

    if (!d)
        return View(a, root, { TitleBar(a, "WI-FI"), Text(a, "No WiFi driver", TextRole::Body) });

    bool on = d->isEnabled();
    append(Toggle(a, "Wi-Fi", on, cbToggleWifi, this));

    if (!on)
        return View(a, root, { TitleBar(a, "WI-FI"), list });

    const char* curSsid = d->isConnected() ? d->ssid() : "";

    auto addRow = [&](const char* ssid, bool secured, Act act, bool saved, bool current, const char* acc) {
        Row r;
        r.self = this; r.ssid = ssid; r.secured = secured;
        r.act = act; r.saved = saved; r.current = current;
        r.label = std::string(ssid) + (secured ? " *" : "");
        r.acc = acc;
        rows_.push_back(std::move(r));
    };

    // ── helper: is this ssid saved? ──
    auto isSaved = [&](const char* ssid) {
        for (size_t i = 0; i < d->savedCount(); i++) {
            WifiProfile p;
            if (d->savedAt(i, p) && std::strcmp(p.ssid, ssid) == 0) return true;
        }
        return false;
    };

    // ── connected network ──
    if (curSsid[0]) addRow(curSsid, false, Act::Detail, true, true, ">");

    // ── saved networks (not the current one) ──
    for (size_t i = 0; i < d->savedCount(); i++) {
        WifiProfile p;
        if (!d->savedAt(i, p)) continue;
        if (curSsid[0] && std::strcmp(p.ssid, curSsid) == 0) continue;
        addRow(p.ssid, p.secured, Act::Detail, true, false, ">");
    }
    size_t savedEnd = rows_.size();

    // ── other (scanned) networks: not current, not saved ──
    for (auto& n : d->scanResults()) {
        if (curSsid[0] && std::strcmp(n.ssid, curSsid) == 0) continue;
        if (isSaved(n.ssid)) continue;
        addRow(n.ssid, n.secured, Act::Join, false, false, bars(n.rssi));
    }

    // ── render with section headers ──
    size_t idx = 0;
    if (curSsid[0]) {
        append(Text(a, "CONNECTED", TextRole::Caption));
        append(ListItem(a, rows_[0].label.c_str(), rows_[0].acc.c_str(), cbPick, &rows_[0]));
        idx = 1;
    }
    if (idx < savedEnd) {
        append(Text(a, "MY NETWORKS", TextRole::Caption));
        for (; idx < savedEnd; idx++)
            append(ListItem(a, rows_[idx].label.c_str(), rows_[idx].acc.c_str(), cbPick, &rows_[idx]));
    }
    append(Text(a, scanning_ ? "OTHER NETWORKS  (scanning...)" : "OTHER NETWORKS", TextRole::Caption));
    if (idx >= rows_.size() && !scanning_)
        append(Text(a, "  No networks found", TextRole::Body));
    for (; idx < rows_.size(); idx++)
        append(ListItem(a, rows_[idx].label.c_str(), rows_[idx].acc.c_str(), cbPick, &rows_[idx]));

    append(ListItem(a, "Add Other Network...", ">", cbAddOther, this));

    return View(a, root, { TitleBar(a, "WI-FI"), list });
}

} // namespace nema
