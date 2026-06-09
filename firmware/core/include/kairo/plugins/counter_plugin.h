#pragma once
#include "kairo/plugin/plugin.h"
#include "kairo/apps/counter_app.h"
#include <memory>

namespace kairo {


// CounterPlugin — now launches Counter as a true app on its own thread.
// onSelect builds an AppHost wrapping CounterApp and pushes it onto the view
// stack. The AppHost spawns the app thread; the app owns its state and loop.
class CounterPlugin : public IPlugin {
public:
    PluginId    id()      const override { return "com.kairo.counter"; }
    const char* name()    const override { return "Counter"; }
    const char* version() const override { return "1.0.0"; }

    CounterPlugin();
    ~CounterPlugin() override;   // out-of-line: AppHost is incomplete here

    void onLoad  (PluginContext& ctx) override;
    void onUnload(PluginContext& /*ctx*/) override {}
    void onSelect(PluginContext& ctx) override;

private:
    CounterApp               app_;
};

} // namespace kairo
