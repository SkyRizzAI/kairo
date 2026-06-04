#include "kairo/plugins/ticker_plugin.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/app/app_host.h"
#include "kairo/log/logger.h"

namespace kairo {

TickerPlugin::TickerPlugin()  = default;
TickerPlugin::~TickerPlugin() = default;

void TickerPlugin::onLoad(PluginContext& ctx) {
    ctx.log().info("TickerPlugin", "loaded (app-threaded)");
}

void TickerPlugin::onSelect(PluginContext& ctx) {
    host_ = std::make_unique<AppHost>(ctx.runtime(), app_);
    ctx.pushScreen(*host_);
}

} // namespace kairo
