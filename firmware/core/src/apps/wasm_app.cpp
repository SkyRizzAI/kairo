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

void WasmApp::requestAbort() {
    WasmEngine::requestAbort();
}

// ── Shared WASM execution ──────────────────────────────────────────────────

void WasmApp::runWasm(ProcessContext& ctx, ISurface* surface) {
    if (wasm_.empty()) {
        outputLines_.push_back("[error: empty module]");
        ctx.runtime().log().error("WasmApp", "empty module", {{"app", id_}});
        done_ = true;
        return;
    }

    WasmEngine engine;
    engine.setPrintHook([this](const std::string& line) {
        outputLines_.push_back(line);
    });

    if (!engine.init(stackBytes() * 3 / 4)) {
        std::string e = "[error: init: " + engine.lastError() + "]";
        outputLines_.push_back(e);
        ctx.runtime().log().error("WasmApp", "engine init failed",
                                  {{"app", id_}, {"err", engine.lastError()}});
        done_ = true;
        return;
    }

    if (!engine.load(wasm_.data(), wasm_.size())) {
        std::string e = "[error: load: " + engine.lastError() + "]";
        outputLines_.push_back(e);
        ctx.runtime().log().error("WasmApp", "module load failed",
                                  {{"app", id_}, {"err", engine.lastError()}});
        done_ = true;
        return;
    }

    int code = engine.runStart(ctx, id_.c_str(), surface);
    if (code != 0 && !ctx.shouldExit()) {
        std::string e = "[exit " + engine.lastError() + "]";
        outputLines_.push_back(e);
        ctx.runtime().log().error("WasmApp", "runStart failed",
                                  {{"app", id_}, {"err", engine.lastError()}});
    }
    done_ = true;
}

// ── UI path (mode=ui) ─────────────────────────────────────────────────────

void WasmApp::onStart(AppContext& ctx) {
    // Reset per-run state — the WasmApp object is reused across launches.
    outputLines_.clear();
    done_        = false;
    displayed_   = false;
    ignoreKeys_  = 1;
    scrollSt_    = {};

    // Run WASM synchronously before the event loop starts. Fast apps (<1ms)
    // complete here; the first build() call will see all output immediately.
    // Pass &ctx as surface: AppContext is ISurface, so canvas_* imports can
    // reach canvas()/present()/enterGuiMode() on the AppHost (Plan 86 Fase 2).
    runWasm(ctx, &ctx);
}

// ── Headless path (mode=cli) ─────────────────────────────────────────────

void WasmApp::runProcess(ProcessContext& ctx) {
    runWasm(ctx);
}

// ── Terminal UI ────────────────────────────────────────────────────────────

UiNode* WasmApp::build(NodeArena& arena, AppContext&) {
    displayed_ = true;
    Style rootSt;
    rootSt.dir      = FlexDir::Col;
    rootSt.flexGrow = 1;
    rootSt.padding  = aether::theme().space.sm;
    rootSt.gap      = 2;

    UiNode* root = View(arena, rootSt, {});
    if (!root) return nullptr;

    // App name as header
    UiNode* title = Text(arena, name_.c_str(), TextRole::Title);
    root->firstChild = title;
    UiNode* prev = title;

    // Output lines — Text nodes linked as siblings
    for (const auto& line : outputLines_) {
        UiNode* t = Text(arena, line.c_str(), TextRole::Body);
        if (t && prev) { prev->nextSibling = t; prev = t; }
    }

    // Footer
    const char* hint = done_ ? "Press any key to exit" : "Running\xe2\x80\xa6";
    UiNode* footer = Text(arena, hint, TextRole::Caption);
    if (footer && prev) prev->nextSibling = footer;

    return root;
}

bool WasmApp::onKey(Key /*k*/, AppContext& ctx) {
    if (ignoreKeys_ > 0) { ignoreKeys_--; return false; }
    if (done_ && displayed_) { ctx.requestExit(0); return true; }
    return false;
}

} // namespace nema
