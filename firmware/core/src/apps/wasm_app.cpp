#include "nema/apps/wasm_app.h"
#include "nema/wasm/wasm_engine.h"
#include "nema/app/app_context.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/ui/widgets.h"
#include "nema/ui/style_tokens.h"
#include <utility>

namespace nema {

using namespace aether::ui;

WasmApp::WasmApp(std::string id, std::string name, std::string version,
                 std::vector<uint8_t> wasm, std::string displayServer)
    : id_(std::move(id)), name_(std::move(name)), version_(std::move(version)),
      displayServer_(std::move(displayServer)), wasm_(std::move(wasm)) {}

WasmApp::~WasmApp() = default;

void WasmApp::setIcon(std::vector<uint8_t> data) {
    if (data.size() < 4) return;
    iconW_ = static_cast<uint8_t>(data[0] | (data[1] << 8));
    iconH_ = static_cast<uint8_t>(data[2] | (data[3] << 8));
    iconData_   = std::move(data);
    iconBitmap_ = iconData_.data() + 4;
}

void WasmApp::runProcess(ProcessContext& ctx) {
    if (wasm_.empty()) {
        ctx.runtime().log().error("WasmApp", "empty module", {{"app", id_}});
        ctx.requestExit(1);
        return;
    }

    WasmEngine engine;
    if (!engine.init(stackBytes() * 3 / 4)) {
        ctx.runtime().log().error("WasmApp", "engine init failed",
                                  {{"app", id_}, {"err", engine.lastError()}});
        ctx.requestExit(1);
        return;
    }

    // wasm3 references wasm_ in place — the vector outlives this call, so no copy.
    if (!engine.load(wasm_.data(), wasm_.size())) {
        ctx.runtime().log().error("WasmApp", "module load failed",
                                  {{"app", id_}, {"err", engine.lastError()}});
        ctx.requestExit(1);
        return;
    }

    int code = engine.runStart(ctx);
    if (code != 0 && !ctx.shouldExit()) {
        ctx.runtime().log().error("WasmApp", "_start failed",
                                  {{"app", id_}, {"err", engine.lastError()}});
        ctx.requestExit(code);
    }
}

aether::ui::UiNode* WasmApp::build(NodeArena& arena, AppContext&) {
    // WASM UI is not implemented yet (Plan 84 Fase 4). A WASM app should declare
    // mode=cli; if it's launched with a surface, show why nothing renders.
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1;
    root.padding = aether::theme().space.sm; root.gap = aether::theme().space.xs;
    return View(arena, root, {
        Text(arena, "WASM UI unsupported", TextRole::Title),
        Text(arena, "This app runs headless (CLI).", TextRole::Caption),
    });
}

} // namespace nema
