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
    JsApp(std::string id, std::string name, std::string bundleJs);
    ~JsApp() override;

    const char* id()   const override { return id_.c_str(); }
    const char* name() const override { return name_.c_str(); }

protected:
    void        onStart(AppContext& ctx) override;
    ui::UiNode* build(ui::NodeArena& arena, AppContext& ctx) override;
    size_t      arenaCapacity() const override { return 1024; }

private:
    std::string id_, name_, js_;
    std::unique_ptr<js::JsEngine> eng_;
    bool        loaded_ = false;
    char        errLine_[96] = "";
};

} // namespace kairo
