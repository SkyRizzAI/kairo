#pragma once
#include "kairo/plugin/plugin.h"
#include "kairo/apps/ticker_app.h"
#include <memory>

namespace kairo {


// TickerPlugin — launches the BTC/USD Ticker as a true app on its own thread.
class TickerPlugin : public IPlugin {
public:
    PluginId    id()      const override { return "com.kairo.ticker"; }
    const char* name()    const override { return "Ticker"; }
    const char* version() const override { return "1.0.0"; }

    TickerPlugin();
    ~TickerPlugin() override;

    void onLoad  (PluginContext& ctx) override;
    void onUnload(PluginContext& /*ctx*/) override {}
    void onSelect(PluginContext& ctx) override;

private:
    TickerApp                app_;
};

} // namespace kairo
