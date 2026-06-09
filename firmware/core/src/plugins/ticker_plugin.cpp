#include "kairo/plugins/ticker_plugin.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/app/app_host_manager.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"

namespace kairo {

TickerPlugin::TickerPlugin()  = default;
TickerPlugin::~TickerPlugin() = default;

void TickerPlugin::onLoad(PluginContext& ctx) {
    ctx.log().info("TickerPlugin", "loaded (app-threaded)");
}

void TickerPlugin::onSelect(PluginContext& ctx) {
    ctx.runtime().apps().launch(app_);
}

} // namespace kairo
