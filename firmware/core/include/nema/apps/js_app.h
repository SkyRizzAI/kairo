#pragma once
#include "nema/app/component_app.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nema::js { class JsEngine; }

namespace nema {

// JsApp — a custom app whose UI/logic is JavaScript (a built .papp bundle),
// running on the embedded QuickJS engine (Plan 37). Being a ComponentApp it gets
// flex layout, focus, scroll, gesture and pause/resume for free; build() renders
// the JS component to the same native UiNode tree as C apps. Runs on the app
// thread (AppHost) so JS GC never touches the UI thread.
class JsApp : public ComponentApp {
public:
    JsApp(std::string id, std::string name, std::string version, std::string bundleJs,
          std::string displayServer = "", std::string category = "");
    ~JsApp() override;

    const char* id()            const override { return id_.c_str(); }
    const char* name()          const override { return name_.c_str(); }
    const char* version()       const { return version_.c_str(); }

    // Launcher group (Launchpad folders). Empty → "Apps" (top-level).
    // Derived from the .papp's subfolder under the app root (or manifest category).
    const char* category()      const override { return category_.empty() ? "Apps" : category_.c_str(); }
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

    // Headless entry point (Plan 84 / Plan 54): run the JS module without a
    // surface. Wires process.argv/stdout/exit then eval()s the module.
    // CLI apps use this path; UI apps use run() → onStart() → build() loop.
    void runProcess(ProcessContext& ctx) override;

    // Set icon from raw bytes (icon.raw format: 4-byte header + 1-bit pixels).
    // Must be called before installCustom() — manifest stores non-owning pointers.
    void setIcon(std::vector<uint8_t> data);

    const uint8_t* iconBitmap() const { return iconBitmap_; }
    uint8_t        iconW()      const { return iconW_; }
    uint8_t        iconH()      const { return iconH_; }

protected:
    void        onStart(AppContext& ctx) override;
    aether::ui::UiNode* build(aether::ui::NodeArena& arena, AppContext& ctx) override;
    size_t      arenaCapacity() const override { return 1024; }

private:
    std::string id_, name_, version_, js_, displayServer_, category_;
    std::unique_ptr<js::JsEngine> eng_;
    bool        loaded_ = false;
    char        errLine_[96] = "";

    // Icon loaded from bundle (icon.raw). iconBitmap_ points into iconData_[4+].
    std::vector<uint8_t> iconData_;
    const uint8_t*       iconBitmap_ = nullptr;
    uint8_t              iconW_      = 0;
    uint8_t              iconH_      = 0;
};

} // namespace nema
