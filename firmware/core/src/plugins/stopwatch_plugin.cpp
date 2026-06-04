#include "kairo/plugins/stopwatch_plugin.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/app/app_host.h"
#include "kairo/log/logger.h"

namespace kairo {

StopwatchPlugin::StopwatchPlugin()  = default;
StopwatchPlugin::~StopwatchPlugin() = default;

void StopwatchPlugin::onLoad(PluginContext& ctx) {
    ctx.log().info("StopwatchPlugin", "loaded (app-threaded)");
}

void StopwatchPlugin::onSelect(PluginContext& ctx) {
    host_ = std::make_unique<AppHost>(ctx.runtime(), app_);
    ctx.pushScreen(*host_);
}

} // namespace kairo
