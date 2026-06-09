#include "kairo/plugins/task_demo_plugin.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/app/app_host_manager.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"

namespace kairo {

TaskDemoPlugin::TaskDemoPlugin()  = default;
TaskDemoPlugin::~TaskDemoPlugin() = default;

void TaskDemoPlugin::onLoad(PluginContext& ctx) {
    ctx.log().info("TaskDemoPlugin", "loaded (app-threaded)");
}

void TaskDemoPlugin::onSelect(PluginContext& ctx) {
    ctx.runtime().apps().launch(app_);
}

} // namespace kairo
