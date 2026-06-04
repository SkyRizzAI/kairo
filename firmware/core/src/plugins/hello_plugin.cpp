#include "kairo/plugins/hello_plugin.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/log/logger.h"
#include "kairo/event/event_bus.h"
#include "kairo/event/event.h"

namespace kairo {

void HelloPlugin::onLoad(PluginContext& ctx) {
    ctx.log().info("HelloPlugin", "loaded — hello from Kairo!");
    ctx.subscribe(events::ClockTick, [](const Event&) {
        // tick acknowledged — no-op, just shows subscription works
    });
}

void HelloPlugin::onUnload(PluginContext& ctx) {
    ctx.log().info("HelloPlugin", "unloaded");
}

void HelloPlugin::onTick(PluginContext& ctx, uint64_t nowMs) {
    if (lastLogMs_ == 0 || nowMs - lastLogMs_ >= 10000) {
        lastLogMs_ = nowMs;
        ctx.log().debug("HelloPlugin", "still running",
            {{"uptimeMs", std::to_string(nowMs)}});
    }
}

} // namespace kairo
