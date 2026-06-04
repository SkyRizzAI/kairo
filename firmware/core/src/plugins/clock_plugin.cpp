#include "kairo/plugins/clock_plugin.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/app/app_host.h"
#include "kairo/log/logger.h"

namespace kairo {

ClockPlugin::ClockPlugin()  = default;
ClockPlugin::~ClockPlugin() = default;

void ClockPlugin::onLoad(PluginContext& ctx) {
    ctx.log().info("ClockPlugin", "loaded (app-threaded)");
}

void ClockPlugin::onSelect(PluginContext& ctx) {
    host_ = std::make_unique<AppHost>(ctx.runtime(), app_);
    ctx.pushScreen(*host_);
}

} // namespace kairo
