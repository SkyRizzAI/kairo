#include "kairo/plugins/stopwatch_plugin.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/app/app_host_manager.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"

namespace kairo {

StopwatchPlugin::StopwatchPlugin()  = default;
StopwatchPlugin::~StopwatchPlugin() = default;

void StopwatchPlugin::onLoad(PluginContext& ctx) {
    ctx.log().info("StopwatchPlugin", "loaded (app-threaded)");
}

void StopwatchPlugin::onSelect(PluginContext& ctx) {
    ctx.runtime().apps().launch(app_);
}

} // namespace kairo
