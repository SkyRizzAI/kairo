#pragma once
#include "kairo/app/component_app.h"
#include <memory>
#include <string>

namespace kairo::js { class JsEngine; }

namespace kairo {

// JsApp — a custom app whose UI/logic is JavaScript (a built .kapp bundle),
// running on the embedded QuickJS engine (Plan 37). Being a ComponentApp it gets
// flex layout, focus, scroll, gesture and pause/resume for free; build() renders
// the JS component to the same native UiNode tree as C apps. Runs on the app
// thread (AppHost) so JS GC never touches the UI thread.
class JsApp : public ComponentApp {
public:
    JsApp(std::string id, std::string name, std::string version, std::string bundleJs);
    ~JsApp() override;

    const char* id()      const override { return id_.c_str(); }
    const char* name()    const override { return name_.c_str(); }
    const char* version() const { return version_.c_str(); }

    // QuickJS compilation uses two nested JS_Eval calls (app module + kairo
    // runtime module), both on the native C stack. Each pass needs ~12 KB;
    // add C overhead and we need at least ~40 KB. 64 KB gives a safe margin.
    uint32_t stackBytes() const override { return 65536; }

protected:
    void        onStart(AppContext& ctx) override;
    ui::UiNode* build(ui::NodeArena& arena, AppContext& ctx) override;
    size_t      arenaCapacity() const override { return 1024; }

private:
    std::string id_, name_, version_, js_;
    std::unique_ptr<js::JsEngine> eng_;
    bool        loaded_ = false;
    char        errLine_[96] = "";
};

} // namespace kairo
