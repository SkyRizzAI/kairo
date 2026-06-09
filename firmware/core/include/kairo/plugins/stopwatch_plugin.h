#pragma once
#include "kairo/plugin/plugin.h"
#include "kairo/apps/stopwatch_app.h"
#include <memory>

namespace kairo {


// StopwatchPlugin — launches Stopwatch as a true app on its own thread.
class StopwatchPlugin : public IPlugin {
public:
    PluginId    id()      const override { return "com.kairo.stopwatch"; }
    const char* name()    const override { return "Stopwatch"; }
    const char* version() const override { return "1.0.0"; }

    StopwatchPlugin();
    ~StopwatchPlugin() override;

    void onLoad  (PluginContext& ctx) override;
    void onUnload(PluginContext& /*ctx*/) override {}
    void onSelect(PluginContext& ctx) override;

private:
    StopwatchApp             app_;
};

} // namespace kairo
