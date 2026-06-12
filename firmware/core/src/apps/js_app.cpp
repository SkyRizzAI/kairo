#include "nema/apps/js_app.h"
#include "nema/js/js_engine.h"
#include "nema/app/app_context.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/ui/widgets.h"
#include <cstdio>
#include <utility>

namespace nema {

using namespace ui;

JsApp::JsApp(std::string id, std::string name, std::string version, std::string bundleJs)
    : id_(std::move(id)), name_(std::move(name)), version_(std::move(version)),
      js_(std::move(bundleJs)) {}

JsApp::~JsApp() = default;

void JsApp::onStart(AppContext& ctx) {
    eng_ = std::make_unique<js::JsEngine>();
    eng_->setMemoryLimit(4 * 1024 * 1024);   // 4 MB JS heap ceiling (PSRAM on device)
    // Coordinate QuickJS's recursion guard with the real thread stack: keep it a
    // safe margin below stackBytes() on EVERY platform. This is the fix for both
    // the simulator "Maximum call stack size exceeded" (guard was left at the 1 MB
    // default, uncoordinated with the WASM worker stack) and the device freeze
    // (overflow corrupted the FreeRTOS stack instead of throwing). Below this a
    // too-deep script throws a clean error → the error card renders, OS survives.
    eng_->setMaxStackSize(stackBytes() * 3 / 4);
    eng_->setDeadlineMs(5000);               // runaway guard per JS turn (time)
    eng_->setHost(&ctx.runtime(), id_);      // capability-gated nema.* system API
    loaded_ = eng_->ok() && eng_->loadApp(js_.c_str(), id_.c_str());
    if (!loaded_) {
        std::snprintf(errLine_, sizeof(errLine_), "JS load failed: %.70s",
                      eng_->lastError().c_str());
        ctx.runtime().log().error("JsApp", errLine_, {{"app", id_}});
    } else {
        ctx.runtime().log().info("JsApp", "loaded", {{"app", id_}});
    }
}

UiNode* JsApp::build(NodeArena& arena, AppContext& ctx) {
    if (!loaded_) {
        // Render a small error card so a broken app is visible, not blank.
        Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 4; root.gap = 4;
        return View(arena, root, {
            Text(arena, "JS app error", TextRole::Title),
            Text(arena, errLine_, TextRole::Caption),
        });
    }
    ctx.runtime().log().debug("JsApp", "render start", {{"app", id_}});
    UiNode* root = eng_->render(arena);
    if (!root) {
        ctx.runtime().log().error("JsApp", "render failed",
                                  {{"app", id_}, {"err", eng_->lastError()}});
        Style s; s.dir = FlexDir::Col; s.flexGrow = 1; s.padding = 4; s.gap = 4;
        return View(arena, s, {
            Text(arena, "render error", TextRole::Title),
            Text(arena, eng_->lastError().size() < 80 ? eng_->lastError().c_str() : "(see logs)",
                 TextRole::Caption),
        });
    }
    ctx.runtime().log().debug("JsApp", "render ok", {{"app", id_}});
    return root;
}

} // namespace nema
