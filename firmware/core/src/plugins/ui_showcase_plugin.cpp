#include "kairo/plugins/ui_showcase_plugin.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/app/app_host_manager.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"

namespace kairo {

UiShowcasePlugin::UiShowcasePlugin()  = default;
UiShowcasePlugin::~UiShowcasePlugin() = default;

void UiShowcasePlugin::onLoad(PluginContext& ctx) {
    ctx.log().info("UiShowcasePlugin", "loaded");
}

void UiShowcasePlugin::onSelect(PluginContext& ctx) {
    ctx.runtime().apps().launch(app_);
}

} // namespace kairo
