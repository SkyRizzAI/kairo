#include "kairo/plugin/plugin_manager.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/event/event_bus.h"
#include "kairo/event/event.h"
#include <algorithm>

namespace kairo {

PluginManager::PluginManager(Runtime& rt) : rt_(rt) {}

void PluginManager::load(IPlugin& plugin) {
    if (isLoaded(plugin.id())) {
        rt_.log().warn("PluginManager",
            std::string("already loaded: ") + plugin.id());
        return;
    }
    auto ctx = std::make_unique<PluginContext>(rt_, plugin);
    plugin.onLoad(*ctx);
    rt_.log().info("PluginManager", std::string("loaded: ") + plugin.name(),
        {{"id", plugin.id()}, {"version", plugin.version()}});
    rt_.events().publish({events::PluginLoaded, {{"id", plugin.id()}, {"name", plugin.name()}}});
    entries_.push_back({&plugin, std::move(ctx)});
}

void PluginManager::unload(PluginId id) {
    auto* e = find(id);
    if (!e) return;
    e->plugin->onUnload(*e->ctx);
    rt_.log().info("PluginManager", std::string("unloaded: ") + id);
    rt_.events().publish({events::PluginUnloaded, {{"id", id}}});
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
        [id](const Entry& x){ return std::string(x.plugin->id()) == id; }),
        entries_.end());
}

void PluginManager::unloadAll() {
    // unload in reverse order
    for (int i = (int)entries_.size() - 1; i >= 0; i--) {
        entries_[i].plugin->onUnload(*entries_[i].ctx);
    }
    entries_.clear();
}

void PluginManager::tickAll(uint64_t nowMs) {
    for (auto& e : entries_) {
        e.plugin->onTick(*e.ctx, nowMs);
    }
}

bool PluginManager::isLoaded(PluginId id) const {
    return std::any_of(entries_.begin(), entries_.end(),
        [id](const Entry& e){ return std::string(e.plugin->id()) == id; });
}

const std::vector<IPlugin*>& PluginManager::plugins() const {
    static std::vector<IPlugin*> result;
    result.clear();
    for (auto& e : entries_) result.push_back(e.plugin);
    return result;
}

void PluginManager::selectPlugin(PluginId id) {
    Entry* e = find(id);
    if (e) e->plugin->onSelect(*e->ctx);
}

PluginManager::Entry* PluginManager::find(PluginId id) {
    for (auto& e : entries_)
        if (std::string(e.plugin->id()) == id) return &e;
    return nullptr;
}

} // namespace kairo
