#pragma once
#include "kairo/plugin/plugin.h"
#include "kairo/apps/ui_showcase_app.h"
#include <memory>

namespace kairo {


// UiShowcasePlugin — exposes the UI Showcase / component test-bench in the Apps
// list. Launches it as a true app on its own thread.
class UiShowcasePlugin : public IPlugin {
public:
    PluginId    id()      const override { return "com.kairo.uishowcase"; }
    const char* name()    const override { return "UI Showcase"; }
    const char* version() const override { return "1.0.0"; }

    UiShowcasePlugin();
    ~UiShowcasePlugin() override;   // out-of-line: AppHost incomplete here

    void onLoad  (PluginContext& ctx) override;
    void onUnload(PluginContext& /*ctx*/) override {}
    void onSelect(PluginContext& ctx) override;

private:
    UiShowcaseApp            app_;
};

} // namespace kairo
