#include "nema/screens/wifi_ip_config_screen.h"
#include "nema/runtime.h"
#include "nema/service/service_container.h"
#include "nema/system/capabilities.h"
#include "nema/system/capability_registry.h"
#include "nema/input/input_code.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/canvas.h"
#include <cstdio>
#include <cstring>

namespace nema {

using namespace aether::ui;

static char* fieldPtr(WifiIpConfig& c, WifiIpConfigScreen::Field f) {
    switch (f) {
        case WifiIpConfigScreen::F_IP:     return c.ip;
        case WifiIpConfigScreen::F_MASK:   return c.mask;
        case WifiIpConfigScreen::F_ROUTER: return c.gw;
        case WifiIpConfigScreen::F_DNS:    return c.dns;
        default:                           return c.ip;
    }
}
static const char* fieldLabel(WifiIpConfigScreen::Field f) {
    switch (f) {
        case WifiIpConfigScreen::F_IP:     return "IP Address";
        case WifiIpConfigScreen::F_MASK:   return "Subnet Mask";
        case WifiIpConfigScreen::F_ROUTER: return "Router";
        case WifiIpConfigScreen::F_DNS:    return "DNS";
        default:                           return "";
    }
}

WifiIpConfigScreen::WifiIpConfigScreen(Runtime& rt) : ComponentScreen(rt, 192) {}

IWifiDriver* WifiIpConfigScreen::drv() {
    if (!drv_) drv_ = rt_.container().resolve<IWifiDriver>();
    return drv_;
}

void WifiIpConfigScreen::redraw() { dirty_ = true; requestRedraw(); }

void WifiIpConfigScreen::onResume() {
    if (IWifiDriver* d = drv()) cfg_ = d->ipConfig();
    manual_ = !cfg_.dhcp;
    st_ = St::Form;
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    redraw();
}

void WifiIpConfigScreen::apply() {
    cfg_.dhcp = !manual_;
    if (IWifiDriver* d = drv()) d->setIpConfig(cfg_);
    rt_.view().goBack();
}

void WifiIpConfigScreen::editField(Field f) {
    editing_ = f;
    st_      = St::EditField;
    kbd_.clear();
    kbd_.setPassword(false);
    kbd_.linear = !rt_.capabilities().has(caps::Input2D);
    const char* cur = fieldPtr(cfg_, f);
    std::strncpy(kbd_.buf, cur, sizeof(kbd_.buf) - 1);
    kbd_.len = (uint8_t)std::strlen(kbd_.buf);
    swallowCode_ = true;
    std::snprintf(prompt_, sizeof(prompt_), "%s", fieldLabel(f));
    redraw();
}

void WifiIpConfigScreen::cbToggleMode(void* u) {
    auto* s = static_cast<WifiIpConfigScreen*>(u);
    s->manual_ = !s->manual_;
    if (s->manual_) {
        WifiIpConfig live = s->drv() ? s->drv()->ipConfig() : WifiIpConfig{};
        if (!s->cfg_.ip[0])   std::strncpy(s->cfg_.ip,   live.ip,   sizeof(s->cfg_.ip) - 1);
        if (!s->cfg_.mask[0]) std::strncpy(s->cfg_.mask, live.mask, sizeof(s->cfg_.mask) - 1);
        if (!s->cfg_.gw[0])   std::strncpy(s->cfg_.gw,   live.gw,   sizeof(s->cfg_.gw) - 1);
        if (!s->cfg_.dns[0])  std::strncpy(s->cfg_.dns,  live.dns,  sizeof(s->cfg_.dns) - 1);
    }
    s->redraw();
}
void WifiIpConfigScreen::cbEdit(void* u) {
    auto* r = static_cast<FieldRow*>(u);
    r->self->editField(r->f);
}
void WifiIpConfigScreen::cbApply(void* u) { static_cast<WifiIpConfigScreen*>(u)->apply(); }

void WifiIpConfigScreen::onAction(input::Action a) {
    if (st_ == St::EditField) {
        swallowCode_ = false;
        if (kbd_.linear) {
            bool done = false, cancel = false;
            kbd_.handleAction(a, done, cancel);
            if (cancel) st_ = St::Form;
            else if (done) { std::strncpy(fieldPtr(cfg_, editing_), kbd_.buf, 15); st_ = St::Form; }
            redraw();
        }
        return;
    }
    ComponentScreen::onAction(a);
}

void WifiIpConfigScreen::onCode(input::Code c) {
    if (st_ != St::EditField || kbd_.linear) return;
    if (swallowCode_) { swallowCode_ = false; return; }
    bool done = false, cancel = false;
    kbd_.handle(input::keyFromCode(c), done, cancel);
    if (cancel) st_ = St::Form;
    else if (done) { std::strncpy(fieldPtr(cfg_, editing_), kbd_.buf, 15); st_ = St::Form; }
    redraw();
}

void WifiIpConfigScreen::draw(Canvas& c) {
    if (st_ == St::EditField) { c.clear(); kbd_.draw(c, prompt_); return; }
    ComponentScreen::draw(c);
}

UiNode* WifiIpConfigScreen::build(NodeArena& a, Runtime& rt) {
    (void)rt;
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    UiNode* list = ListContainer(a, scroll_, {});
    UiNode* prev = nullptr;
    auto append = [&](UiNode* n) {
        if (!n) return;
        if (!prev) list->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };

    {
        ListEntry e;
        e.label   = "Configure IP";
        e.value   = manual_ ? "Manual" : "Automatic";
        e.onPress = cbToggleMode; e.user = this;
        append(ListItemRow(a, e));
    }

    if (manual_) {
        append(ListSection(a, "Manual Configuration"));
        for (int i = 0; i < F_COUNT; i++) {
            Field f = (Field)i;
            fieldRows_[i].self = this;
            fieldRows_[i].f    = f;
            const char* v = fieldPtr(cfg_, f);
            fieldRows_[i].text = v[0] ? v : "(tap to set)";
            ListEntry e;
            e.label   = fieldLabel(f);
            e.value   = fieldRows_[i].text.c_str();
            e.chevron = true;
            e.onPress = cbEdit; e.user = &fieldRows_[i];
            append(ListItemRow(a, e));
        }
    } else {
        ListEntry e; e.label = "Uses DHCP from the router";
        append(ListItemRow(a, e));
    }

    {
        ListEntry e; e.label = "Apply"; e.chevron = true;
        e.onPress = cbApply; e.user = this;
        append(ListItemRow(a, e));
    }

    return View(a, root, { TitleBar(a, "Configure IP"), list });
}

} // namespace nema
