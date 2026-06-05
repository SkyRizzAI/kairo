#include "kairo/plugins/ui_showcase_plugin.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/app/app_host.h"
#include "kairo/log/logger.h"

namespace kairo {

UiShowcasePlugin::UiShowcasePlugin()  = default;
UiShowcasePlugin::~UiShowcasePlugin() = default;

void UiShowcasePlugin::onLoad(PluginContext& ctx) {
    ctx.log().info("UiShowcasePlugin", "loaded");
}

void UiShowcasePlugin::onSelect(PluginContext& ctx) {
    host_ = std::make_unique<AppHost>(ctx.runtime(), app_);
    ctx.pushScreen(*host_);
}

} // namespace kairo
