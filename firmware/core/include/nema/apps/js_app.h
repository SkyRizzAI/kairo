#pragma once
#include "nema/app/component_app.h"
#include <memory>
#include <string>

namespace nema::js { class JsEngine; }

namespace nema {

// JsApp — a custom app whose UI/logic is JavaScript (a built .kapp bundle),
// running on the embedded QuickJS engine (Plan 37). Being a ComponentApp it gets
// flex layout, focus, scroll, gesture and pause/resume for free; build() renders
// the JS component to the same native UiNode tree as C apps. Runs on the app
// thread (AppHost) so JS GC never touches the UI thread.
class JsApp : public ComponentApp {
public:
    JsApp(std::string id, std::string name, std::string version, std::string bundleJs,
          std::string displayServer = "");
    ~JsApp() override;

    const char* id()            const override { return id_.c_str(); }
    const char* name()          const override { return name_.c_str(); }
    const char* version()       const { return version_.c_str(); }
    const char* displayServer() const { return displayServer_.empty() ? nullptr : displayServer_.c_str(); }

    // App thread stack (FreeRTOS task on ESP; pthread on host/WASM — see
    // thread_host.cpp, which now honours this). QuickJS's overflow guard is set
    // strictly below this (see js_app.cpp), so a runaway/too-deep script throws a
    // clean catchable error — never corrupting the real stack or freezing the OS.
    // ESP keeps a modest internal-RAM stack; host/WASM can afford a large one.
    // Once apps load as bytecode (no on-device parser) even the ESP budget is
    // comfortable.
    uint32_t stackBytes() const override {
#ifdef ESP_PLATFORM
        // The cost of loading a JS app is QuickJS *module evaluation* (interpreter
        // recursion), not parsing — so it can't be shrunk by precompiling. Give the
        // app thread a generous stack with the guard (3/4 = 192 KB) safely below
        // it. 128 KB wouldn't fit internal RAM, so this stack lives in PSRAM
        // (thread_esp32.cpp routes large stacks to MALLOC_CAP_SPIRAM — the board
        // sets CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY).
        return 256 * 1024;
#else
        return 512 * 1024;
#endif
    }

protected:
    void        onStart(AppContext& ctx) override;
    ui::UiNode* build(ui::NodeArena& arena, AppContext& ctx) override;
    size_t      arenaCapacity() const override { return 1024; }

private:
    std::string id_, name_, version_, js_, displayServer_;
    std::unique_ptr<js::JsEngine> eng_;
    bool        loaded_ = false;
    char        errLine_[96] = "";
};

} // namespace nema
