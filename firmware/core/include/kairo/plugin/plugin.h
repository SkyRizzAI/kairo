#pragma once
#include <cstdint>

namespace kairo {

class PluginContext;
using PluginId = const char*;

struct IPlugin {
    virtual ~IPlugin() = default;
    virtual PluginId    id()      const = 0;  // "com.kairo.hello"
    virtual const char* name()    const = 0;
    virtual const char* version() const = 0;  // "1.0.0"
    virtual void onLoad  (PluginContext& ctx) = 0;
    virtual void onUnload(PluginContext& ctx) = 0;
    virtual void onTick  (PluginContext& /*ctx*/, uint64_t /*nowMs*/) {}
    virtual void onSelect(PluginContext& /*ctx*/) {}
};

} // namespace kairo
