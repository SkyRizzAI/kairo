#include "kairo/plugins/counter_plugin.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/app/app_host.h"
#include "kairo/log/logger.h"

namespace kairo {

// Defined here where AppHost is a complete type (unique_ptr dtor needs it).
CounterPlugin::CounterPlugin()  = default;
CounterPlugin::~CounterPlugin() = default;

void CounterPlugin::onLoad(PluginContext& ctx) {
    ctx.log().info("CounterPlugin", "loaded (app-threaded)");
}

void CounterPlugin::onSelect(PluginContext& ctx) {
    // Build a fresh AppHost wrapping the app and push it. AppHost::enter()
    // spawns the app thread; AppHost::tick() pops itself when the app exits.
    host_ = std::make_unique<AppHost>(ctx.runtime(), app_);
    ctx.pushScreen(*host_);
}

} // namespace kairo
