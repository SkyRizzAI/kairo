#include "kairo/plugins/task_demo_plugin.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/app/app_host.h"
#include "kairo/log/logger.h"

namespace kairo {

TaskDemoPlugin::TaskDemoPlugin()  = default;
TaskDemoPlugin::~TaskDemoPlugin() = default;

void TaskDemoPlugin::onLoad(PluginContext& ctx) {
    ctx.log().info("TaskDemoPlugin", "loaded (app-threaded)");
}

void TaskDemoPlugin::onSelect(PluginContext& ctx) {
    host_ = std::make_unique<AppHost>(ctx.runtime(), app_);
    ctx.pushScreen(*host_);
}

} // namespace kairo
