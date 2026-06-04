#pragma once
#include "kairo/plugin/plugin.h"
#include <vector>
#include <memory>

namespace kairo {

class Runtime;
class PluginContext;

class PluginManager {
public:
    explicit PluginManager(Runtime& rt);

    void load        (IPlugin& plugin);
    void unload      (PluginId id);
    void unloadAll   ();
    void tickAll     (uint64_t nowMs);
    void selectPlugin(PluginId id);

    bool isLoaded(PluginId id) const;
    const std::vector<IPlugin*>& plugins() const;

private:
    struct Entry {
        IPlugin*                      plugin;
        std::unique_ptr<PluginContext> ctx;
    };

    Runtime&          rt_;
    std::vector<Entry> entries_;

    Entry* find(PluginId id);
};

} // namespace kairo
