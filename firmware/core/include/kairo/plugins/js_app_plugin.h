#pragma once
#include "kairo/plugin/plugin.h"
#include "kairo/apps/js_app.h"
#include <string>

namespace kairo {

// JsAppPlugin — exposes a JS custom app (a built .kapp) in the Apps list. The
// "Embedded app store" registers one of these per built-in app (Plan 37 Fase 5);
// the OTA store (Fase 6) creates them from installed bundles. Launches via the
// AppHostManager so JS apps get pause/resume like native apps.
class JsAppPlugin : public IPlugin {
public:
    JsAppPlugin(std::string id, std::string name, std::string js);
    ~JsAppPlugin() override;

    PluginId    id()      const override { return id_.c_str(); }
    const char* name()    const override { return name_.c_str(); }
    const char* version() const override { return "1.0.0"; }

    void onLoad  (PluginContext& /*ctx*/) override {}
    void onUnload(PluginContext& /*ctx*/) override {}
    void onSelect(PluginContext& ctx) override;

private:
    std::string id_, name_;
    JsApp       app_;
};

} // namespace kairo
