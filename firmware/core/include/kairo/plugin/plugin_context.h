#pragma once
#include "kairo/event/event_bus.h"
#include <vector>

namespace kairo {

class Runtime;
class Logger;
class EventBus;
class CapabilityRegistry;
class ServiceContainer;
struct IService;
struct IPlugin;
struct IScreen;

class PluginContext {
public:
    PluginContext(Runtime& rt, IPlugin& plugin);
    ~PluginContext();  // auto-unsubscribe all event subscriptions

    Runtime&            runtime();      // full runtime access (tasks, clock, ...)
    Logger&             log();
    EventBus&           events();
    CapabilityRegistry& capabilities();
    ServiceContainer&   container();

    // Subscribe — auto-unsubscribed when plugin unloads
    SubscriptionId subscribe(const char* name, EventHandler handler);

    // Screen navigation (requires display capability)
    void pushScreen(IScreen& screen);
    void popScreen();
    void requestRedraw();

    // Register a background service on behalf of this plugin
    void registerService(IService* svc);

    IPlugin& plugin() { return plugin_; }

private:
    Runtime&  rt_;
    IPlugin&  plugin_;
    std::vector<SubscriptionId> subscriptions_;
};

} // namespace kairo
