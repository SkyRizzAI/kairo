#include "nema/apps/ticker_app.h"
#include "nema/app/app_context.h"
#include "nema/runtime.h"
#include "nema/clock.h"
#include "nema/service/service_container.h"
#include "nema/hal/http_client.h"
#include "nema/task_runner.h"
#include "nema/ui/widgets.h"
#include <string>
#include <cstdio>

namespace nema {

using namespace ui;

static const char* BINANCE_URL =
    "https://data-api.binance.vision/api/v3/ticker/price?symbol=BTCUSDT";

// crude JSON: {"symbol":"BTCUSDT","price":"43210.50000000"}
static std::string parsePrice(const std::string& body) {
    auto p = body.find("\"price\":\"");
    if (p == std::string::npos) return "";
    p += 9;
    auto e = body.find('"', p);
    if (e == std::string::npos) return "";
    std::string s = body.substr(p, e - p);
    auto dot = s.find('.');
    if (dot != std::string::npos && s.size() > dot + 3) s = s.substr(0, dot + 3);
    return s;
}

void TickerApp::startFetch(AppContext& ctx) {
    if (!http_) { status_ = "No HTTP driver"; return; }
    fetching_ = true; status_ = "Fetching";
    sh_ = std::make_shared<Shared>();
    auto s = sh_;
    IHttpClient* http = http_;
    ctx.runtime().tasks().submit(
        [http, s]() {                                   // worker thread (blocks)
            HttpResponse r = http->get(BINANCE_URL, true);
            if (r.ok()) {
                std::string pr = parsePrice(r.body);
                s->price  = pr.empty() ? "parse err" : pr;
                s->status = pr.empty() ? "bad data"  : "BTC/USD";
            } else {
                s->price  = "---";
                s->status = r.status ? ("HTTP " + std::to_string(r.status)) : "No net";
            }
            s->done.store(true);
        },
        []{ /* poll the flag on the app thread instead */ });
}

void TickerApp::onStart(AppContext& ctx) {
    http_ = ctx.runtime().container().resolve<IHttpClient>();
    startFetch(ctx);   // auto-fetch on open
}

bool TickerApp::onKey(Key k, AppContext& ctx) {
    if (k == Key::Select && !fetching_) { startFetch(ctx); return true; }
    return false;   // Cancel → base exits
}

bool TickerApp::onTick(AppContext& ctx) {
    bool changed = false;
    if (fetching_ && sh_ && sh_->done.load()) {
        price_ = sh_->price; status_ = sh_->status;
        fetching_ = false; sh_.reset();
        changed = true;
    }
    if (fetching_) {
        uint64_t now = ctx.runtime().clock().millis();
        if (now - lastSpin_ >= 150) { lastSpin_ = now; spin_ = (spin_ + 1) % 4; changed = true; }
    }
    return changed;
}

UiNode* TickerApp::build(NodeArena& a, AppContext&) {
    std::snprintf(priceBuf_, sizeof(priceBuf_), "$%s", price_.c_str());

    const char SP[] = {'|','/','-','\\'};
    if (fetching_) std::snprintf(statusBuf_, sizeof(statusBuf_), "%s %c (UI alive)", status_.c_str(), SP[spin_]);
    else           std::snprintf(statusBuf_, sizeof(statusBuf_), "%s", status_.c_str());

    Style root;
    root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 4; root.gap = 6;
    root.align = Align::Center; root.justify = Justify::Center;

    return View(a, root, {
        Text(a, priceBuf_, TextRole::Title),
        Text(a, statusBuf_, TextRole::Body),
        Text(a, "OK=refresh  Cancel=back", TextRole::Caption),
    });
}

} // namespace nema
