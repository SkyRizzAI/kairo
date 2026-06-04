#include "kairo/plugins/camera_plugin.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/apps/camera_app.h"
#include "kairo/log/logger.h"
#include "kairo/system/capability_registry.h"

namespace kairo {

CameraPlugin::CameraPlugin()  = default;
CameraPlugin::~CameraPlugin() = default;

void CameraPlugin::onLoad(PluginContext& ctx) {
    app_ = std::make_unique<CameraApp>(ctx.runtime());
    if (!ctx.capabilities().has("camera"))
        ctx.log().warn("CameraPlugin", "no camera capability");
}

void CameraPlugin::onSelect(PluginContext& ctx) {
    ctx.pushScreen(*app_);
}

} // namespace kairo
