#pragma once
#include "kairo/plugin/plugin.h"

namespace kairo {

class HelloPlugin : public IPlugin {
public:
    PluginId    id()      const override { return "com.kairo.hello"; }
    const char* name()    const override { return "Hello Plugin"; }
    const char* version() const override { return "1.0.0"; }

    void onLoad  (PluginContext& ctx) override;
    void onUnload(PluginContext& ctx) override;
    void onTick  (PluginContext& ctx, uint64_t nowMs) override;
private:
    uint64_t lastLogMs_ = 0;
};

} // namespace kairo
