#include "kairo/plugins/js_app_plugin.h"
#include "kairo/plugin/plugin_context.h"
#include "kairo/app/app_host_manager.h"
#include "kairo/runtime.h"
#include <utility>

namespace kairo {

JsAppPlugin::JsAppPlugin(std::string id, std::string name, std::string js)
    : id_(std::move(id)), name_(std::move(name)), app_(id_, name_, std::move(js)) {}

JsAppPlugin::~JsAppPlugin() = default;

void JsAppPlugin::onSelect(PluginContext& ctx) {
    ctx.runtime().apps().launch(app_);
}

} // namespace kairo
