#pragma once
#include "kairo/plugin/plugin.h"
#include "kairo/apps/task_demo_app.h"
#include <memory>

namespace kairo {


// TaskDemoPlugin — launches the TaskDemo app on its own thread.
class TaskDemoPlugin : public IPlugin {
public:
    PluginId    id()      const override { return "com.kairo.taskdemo"; }
    const char* name()    const override { return "Task Demo"; }
    const char* version() const override { return "1.0.0"; }

    TaskDemoPlugin();
    ~TaskDemoPlugin() override;

    void onLoad  (PluginContext& ctx) override;
    void onUnload(PluginContext& /*ctx*/) override {}
    void onSelect(PluginContext& ctx) override;

private:
    TaskDemoApp              app_;
};

} // namespace kairo
