#pragma once
#include "nema/app/component_app.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace nema {

// WasmApp — a custom app whose code is a WebAssembly module (a built .papp
// bundle), running on the embedded wasm3 interpreter (Plan 57/84).
//
// Today only the HEADLESS path is implemented: runProcess() loads the module,
// links the WASI imports (argv/stdout/stderr/exit) to the ProcessContext, and
// calls `_start`. This makes WASM CLI apps fully on par with C and JS CLI apps.
//
// UI mode is NOT yet supported (it needs the aether ABI exposed as wasm imports
// — Plan 84 Fase 4). If a WASM app is launched as UI, build() renders an error
// card so it degrades visibly instead of crashing.
//
// The module bytes are owned here (wasm_) and outlive every engine call —
// wasm3 references the input buffer rather than copying it.
class WasmApp : public ComponentApp {
public:
    WasmApp(std::string id, std::string name, std::string version,
            std::vector<uint8_t> wasm, std::string displayServer = "");
    ~WasmApp() override;

    const char* id()            const override { return id_.c_str(); }
    const char* name()          const override { return name_.c_str(); }
    const char* version()       const { return version_.c_str(); }
    const char* displayServer() const { return displayServer_.empty() ? nullptr : displayServer_.c_str(); }

    // wasm3's interpreter recurses; give it a comfortable native stack.
    uint32_t stackBytes() const override {
#ifdef ESP_PLATFORM
        return 64 * 1024;
#else
        return 256 * 1024;
#endif
    }

    // UI path (mode=ui): runs WASM synchronously in onStart() then shows
    // captured output on screen. Any key press exits after WASM completes.
    void onStart(AppContext& ctx) override;

    // Headless path (mode=cli): runs WASM with no surface. Output goes to logs.
    void runProcess(ProcessContext& ctx) override;

    // Icon from bundle (icon.raw: 4-byte header + 1-bit pixels). Same contract
    // as JsApp — manifest stores non-owning pointers into iconData_.
    void setIcon(std::vector<uint8_t> data);
    const uint8_t* iconBitmap() const { return iconBitmap_; }
    uint8_t        iconW()      const { return iconW_; }
    uint8_t        iconH()      const { return iconH_; }

protected:
    aether::ui::UiNode* build(aether::ui::NodeArena& arena, AppContext& ctx) override;
    bool onKey(Key k, AppContext& ctx) override;
    size_t arenaCapacity() const override { return 512; }

private:
    void runWasm(ProcessContext& ctx);   // shared impl for both paths

    std::string          id_, name_, version_, displayServer_;
    std::vector<uint8_t> wasm_;

    std::vector<uint8_t> iconData_;
    const uint8_t*       iconBitmap_ = nullptr;
    uint8_t              iconW_      = 0;
    uint8_t              iconH_      = 0;

    // Terminal output (UI path only). Populated by nema_print() via printHook.
    std::vector<std::string> outputLines_;
    std::atomic<bool>        done_{false};
    aether::ui::ScrollState  scrollSt_;
};

} // namespace nema
