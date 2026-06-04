#pragma once
#include "kairo/plugin/plugin.h"
#include <memory>

namespace kairo {

class CameraApp;
class PluginContext;

class CameraPlugin : public IPlugin {
public:
    CameraPlugin();
    ~CameraPlugin() override;

    PluginId    id()      const override { return "com.kairo.camera"; }
    const char* name()    const override { return "Camera"; }
    const char* version() const override { return "1.0.0"; }

    void onLoad  (PluginContext& ctx) override;
    void onUnload(PluginContext& /*ctx*/) override {}
    void onSelect(PluginContext& ctx) override;

private:
    std::unique_ptr<CameraApp> app_;
};

} // namespace kairo
