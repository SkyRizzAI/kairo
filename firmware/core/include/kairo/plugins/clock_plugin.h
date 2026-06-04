#pragma once
#include "kairo/plugin/plugin.h"
#include "kairo/apps/clock_app.h"
#include <memory>

namespace kairo {

class AppHost;

// ClockPlugin — launches Clock as a true app on its own thread.
class ClockPlugin : public IPlugin {
public:
    PluginId    id()      const override { return "com.kairo.clock"; }
    const char* name()    const override { return "Clock"; }
    const char* version() const override { return "1.0.0"; }

    ClockPlugin();
    ~ClockPlugin() override;

    void onLoad  (PluginContext& ctx) override;
    void onUnload(PluginContext& /*ctx*/) override {}
    void onSelect(PluginContext& ctx) override;

private:
    ClockApp                 app_;
    std::unique_ptr<AppHost> host_;
};

} // namespace kairo
