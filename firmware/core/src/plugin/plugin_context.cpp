#include "kairo/plugin/plugin_context.h"
#include "kairo/plugin/plugin.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/event/event_bus.h"
#include "kairo/system/capability_registry.h"
#include "kairo/service/service_container.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/ui/screen.h"

namespace kairo {

PluginContext::PluginContext(Runtime& rt, IPlugin& plugin)
    : rt_(rt), plugin_(plugin) {}

PluginContext::~PluginContext() {
    for (auto id : subscriptions_) {
        rt_.events().unsubscribe(id);
    }
}

Runtime& PluginContext::runtime()       { return rt_; }
Logger& PluginContext::log()            { return rt_.log(); }
EventBus& PluginContext::events()       { return rt_.events(); }
CapabilityRegistry& PluginContext::capabilities() { return rt_.capabilities(); }
ServiceContainer& PluginContext::container()      { return rt_.container(); }

SubscriptionId PluginContext::subscribe(const char* name, EventHandler handler) {
    auto id = rt_.events().subscribe(name, std::move(handler));
    subscriptions_.push_back(id);
    return id;
}

void PluginContext::pushScreen(IScreen& screen) {
    if (rt_.capabilities().has("display")) {
        rt_.view().push(screen);
        rt_.view().requestRedraw();
    }
}

void PluginContext::popScreen() {
    if (rt_.capabilities().has("display")) {
        rt_.view().pop();
        rt_.view().requestRedraw();
    }
}

void PluginContext::requestRedraw() {
    if (rt_.capabilities().has("display")) {
        rt_.view().requestRedraw();
    }
}

void PluginContext::registerService(IService* svc) {
    rt_.container().registerService(svc);
}

} // namespace kairo
