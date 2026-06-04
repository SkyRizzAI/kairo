#include "kairo/apps/wifi_app.h"
#include "kairo/app/app_context.h"
#include "kairo/runtime.h"
#include "kairo/service/service_container.h"
#include "kairo/hal/wifi.h"
#include "kairo/config/config_store.h"
#include "kairo/nema/task_runner.h"
#include "kairo/ui/widgets.h"
#include "kairo/ui/canvas.h"
#include <cstdio>
#include <cstring>

namespace kairo {

using namespace ui;

// ── async ops on the TaskRunner worker ─────────────────────────────────────

void WifiApp::startScan() {
    if (!drv_ || !ctx_) return;
    scanDone_ = std::make_shared<std::atomic<bool>>(false);
    auto d = scanDone_; IWifiDriver* drv = drv_;
    state_ = St::Scanning;
    ctx_->runtime().tasks().submit([drv]{ drv->scan(); }, [d]{ d->store(true); });
}

void WifiApp::startConnect(const std::string& ssid, const std::string& pw) {
    if (!drv_ || !ctx_) return;
    connectDone_ = std::make_shared<std::atomic<bool>>(false);
    auto d = connectDone_; IWifiDriver* drv = drv_;
    std::string s = ssid, p = pw;
    state_ = St::Connecting;
    ctx_->runtime().tasks().submit([drv, s, p]{ drv->connect(s.c_str(), p.c_str()); },
                                   [d]{ d->store(true); });
}

void WifiApp::pickNetwork(int idx) {
    if (idx < 0 || idx >= (int)nets_.size()) return;
    pendingSsid_ = nets_[idx].ssid;
    if (nets_[idx].secured) {
        pass_.clear(); pass_.mask = true;
        if (cfg_) {   // pre-fill saved password when the SSID matches
            std::string ss, pw;
            if (cfg_->getString("wifi", "ssid", ss) && ss == pendingSsid_ &&
                cfg_->getString("wifi", "password", pw) && !pw.empty()) {
                std::strncpy(pass_.buf, pw.c_str(), sizeof(pass_.buf) - 1);
                pass_.buf[sizeof(pass_.buf) - 1] = '\0';
                pass_.len = (int)pw.size();
            }
        }
        state_ = St::EnterPass;
    } else {
        startConnect(pendingSsid_, "");
    }
}

// ── menu callbacks ─────────────────────────────────────────────────────────

void WifiApp::cbScan(void* u)        { static_cast<WifiApp*>(u)->startScan(); }
void WifiApp::cbDisconnect(void* u)  { auto* s = static_cast<WifiApp*>(u); if (s->drv_) s->drv_->disconnect(); }
void WifiApp::cbIpSettings(void* u)  { auto* s = static_cast<WifiApp*>(u); s->ok_ = true; s->state_ = St::Result; }
void WifiApp::cbPickNet(void* u)     { auto* pc = static_cast<PickCtx*>(u); pc->self->pickNetwork(pc->idx); }

// ── lifecycle / input / tick ─────────────────────────────────────────────────

void WifiApp::onStart(AppContext& ctx) {
    ctx_ = &ctx;
    drv_ = ctx.runtime().container().resolve<IWifiDriver>();
    cfg_ = ctx.runtime().container().resolve<IConfigStore>();
}

bool WifiApp::onKey(Key k, AppContext&) {
    switch (state_) {
    case St::EnterPass: {
        bool done = false, cancel = false;
        pass_.handle(k, done, cancel);
        if (cancel)    { state_ = St::Pick; return true; }
        if (done)      { startConnect(pendingSsid_, std::string(pass_.buf, (size_t)pass_.len)); return true; }
        return true;   // any key redraws the keyboard
    }
    case St::Result:
        if (k == Key::Select || k == Key::Cancel) { state_ = St::Overview; return true; }
        return false;
    case St::Pick:
    case St::Scanning:
    case St::Connecting:
        if (k == Key::Cancel) { state_ = St::Overview; return true; }
        return false;
    case St::Overview:
    default:
        return false;   // Cancel → base exits; Select handled by Menu focus
    }
}

bool WifiApp::onTick(AppContext&) {
    if (state_ == St::Scanning && scanDone_ && scanDone_->load()) {
        nets_  = drv_->scanResults();
        state_ = St::Pick;
        return true;
    }
    if (state_ == St::Connecting && connectDone_ && connectDone_->load()) {
        ok_ = drv_->isConnected();
        if (ok_ && cfg_) {
            cfg_->setString("wifi", "ssid",     pendingSsid_);
            cfg_->setString("wifi", "password", std::string(pass_.buf, (size_t)pass_.len));
        }
        state_ = St::Result;
        return true;
    }
    return false;
}

bool WifiApp::drawRaw(Canvas& c, AppContext&) {
    if (state_ != St::EnterPass) return false;
    std::snprintf(passPrompt_, sizeof(passPrompt_), "Password: %s", pendingSsid_.c_str());
    pass_.draw(c, passPrompt_);
    return true;
}

// ── component tree per state ─────────────────────────────────────────────────

UiNode* WifiApp::build(NodeArena& a, AppContext&) {
    Style root;
    root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 4; root.gap = 4;
    root.align = Align::Stretch;

    if (!drv_)
        return View(a, root, { Header(a, "WiFi"), Text(a, "No WiFi driver", TextRole::Body) });

    switch (state_) {
    case St::Overview: {
        if (drv_->isConnected())
            std::snprintf(statusBuf_, sizeof(statusBuf_), "Connected: %s", drv_->ssid());
        else
            std::snprintf(statusBuf_, sizeof(statusBuf_), "Disconnected");
        MenuItem items[3] = {
            { "Scan Networks", cbScan,       this },
            { "Disconnect",    cbDisconnect, this },
            { "IP Settings",   cbIpSettings, this },
        };
        return View(a, root, {
            Header(a, "WiFi"),
            Text(a, statusBuf_, TextRole::Body),
            Menu(a, items, 3),
        });
    }
    case St::Scanning:
        return View(a, root, { Header(a, "WiFi"), Text(a, "Scanning...", TextRole::Body) });

    case St::Pick: {
        if (nets_.empty())
            return View(a, root, { Header(a, "Networks"), Text(a, "No networks", TextRole::Body) });
        MenuItem items[16];
        int n = (int)nets_.size(); if (n > 16) n = 16;
        for (int i = 0; i < n; i++) {
            pickCtx_[i] = { this, i };
            items[i] = { nets_[i].ssid, cbPickNet, &pickCtx_[i] };
        }
        return View(a, root, { Header(a, "Networks"), Menu(a, items, n) });
    }
    case St::Connecting:
        return View(a, root, { Header(a, "WiFi"), Text(a, "Connecting...", TextRole::Body) });

    case St::Result:
        return View(a, root, {
            Header(a, "WiFi"),
            Text(a, drv_->isConnected() ? "Connected" : "Not connected", TextRole::Body),
            Text(a, "OK/Cancel to continue", TextRole::Caption),
        });

    case St::EnterPass:
    default:
        return nullptr;   // painted by drawRaw()
    }
}

} // namespace kairo
