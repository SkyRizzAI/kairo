#include "kairo/apps/js_app.h"
#include "kairo/js/js_engine.h"
#include "kairo/app/app_context.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/ui/widgets.h"
#include <cstdio>
#include <utility>

namespace kairo {

using namespace ui;

JsApp::JsApp(std::string id, std::string name, std::string version, std::string bundleJs)
    : id_(std::move(id)), name_(std::move(name)), version_(std::move(version)),
      js_(std::move(bundleJs)) {}

JsApp::~JsApp() = default;

void JsApp::onStart(AppContext& ctx) {
    eng_ = std::make_unique<js::JsEngine>();
    eng_->setMemoryLimit(4 * 1024 * 1024);   // 4 MB JS heap ceiling (PSRAM on device)
    eng_->setDeadlineMs(5000);               // runaway guard per JS turn
    eng_->setHost(&ctx.runtime(), id_);      // capability-gated kairo.* system API
    loaded_ = eng_->ok() && eng_->loadApp(js_.c_str(), id_.c_str());
    if (!loaded_) {
        std::snprintf(errLine_, sizeof(errLine_), "JS load failed: %.70s",
                      eng_->lastError().c_str());
        ctx.runtime().log().error("JsApp", errLine_, {{"app", id_}});
    } else {
        ctx.runtime().log().info("JsApp", "loaded", {{"app", id_}});
    }
}

UiNode* JsApp::build(NodeArena& arena, AppContext&) {
    if (!loaded_) {
        // Render a small error card so a broken app is visible, not blank.
        Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 4; root.gap = 4;
        return View(arena, root, {
            Text(arena, "JS app error", TextRole::Title),
            Text(arena, errLine_, TextRole::Caption),
        });
    }
    UiNode* root = eng_->render(arena);
    if (!root) {
        Style s; s.padding = 4;
        return View(arena, s, { Text(arena, "render error", TextRole::Body) });
    }
    return root;
}

} // namespace kairo
