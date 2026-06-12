#pragma once
#include "nema/app/component_app.h"
#include <atomic>
#include <memory>
#include <string>

namespace nema {

struct IHttpClient;

// TickerApp — BTC/USD ticker on the component system (Plan 27). The HTTP fetch
// (1-3s, blocking) runs on a TaskRunner worker; the app polls the result in
// onTick and animates a spinner — UI never freezes (the Nema kernel's point).
class TickerApp : public ComponentApp {
public:
    const char* id()   const override { return "com.palanu.ticker"; }
    const char* name() const override { return "Ticker"; }

protected:
    void        onStart(AppContext& ctx) override;            // auto-fetch on open
    ui::UiNode* build(ui::NodeArena& a, AppContext& ctx) override;
    bool        onKey(Key k, AppContext& ctx) override;       // Select = refresh
    uint32_t    tickIntervalMs() const override { return 100; }
    bool        onTick(AppContext& ctx) override;             // poll result + spinner

private:
    struct Shared { std::atomic<bool> done{false}; std::string price, status; };

    IHttpClient*            http_   = nullptr;
    std::string             price_  = "---";
    std::string             status_ = "Press OK to fetch";
    bool                    fetching_ = false;
    int                     spin_   = 0;
    uint64_t                lastSpin_ = 0;
    std::shared_ptr<Shared> sh_;
    char                    priceBuf_[40] = "$---";
    char                    statusBuf_[48] = "";

    void startFetch(AppContext& ctx);
};

} // namespace nema
