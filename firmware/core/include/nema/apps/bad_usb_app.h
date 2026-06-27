#pragma once
#include "nema/app/component_app.h"
#include "nema/apps/badusb_parser.h"
#include "nema/ui/widgets.h"
#include "nema/ui/virtual_list.h"
#include <string>
#include <vector>

namespace nema {

class  Runtime;
struct IFileSystem;
struct IUsbHid;
class  AppContext;

// BadUSB script executor — runs as a proper app on its own thread (Plan 84 Fase 3).
// UI state machine:
//   kMain       — settings overview + "Run Script" nav row
//   kScriptList — VirtualList of .dd scripts; capturesInput() routes nav to onKey()
//   kRunning    — progress while the script executes; Back cancels
class BadUsbApp : public ComponentApp {
public:
    const char* id()       const override { return "com.palanu.badusb"; }
    const char* name()     const override { return "BadUSB"; }
    const char* category() const override { return "System"; }
    bool fullscreen()      const override { return false; }

protected:
    void onStart(AppContext& ctx) override;
    aether::ui::UiNode* build(aether::ui::NodeArena& arena, AppContext& ctx) override;
    bool onKey(Key k, AppContext& ctx) override;
    bool onTick(AppContext& ctx) override;
    uint32_t tickIntervalMs() const override { return running_ ? 20u : 0u; }
    // capturesInput: bypass component-tree focus in kScriptList so onKey() owns nav.
    bool capturesInput()   const override { return running_ || state_ == kScriptList; }
    size_t arenaCapacity() const override { return 256; }

private:
    enum State { kMain, kScriptList, kConfirm, kRunning, kError };

    void scanScripts(IFileSystem* fs);
    void startExecution(IFileSystem* fs, Runtime& rt);
    void execNextCommand();
    void selectFocused(AppContext& ctx);

    static void cbRunScript(void* u);
    static void cbConfirmRun(void* u);     // confirmed → start the (offensive) script
    static void cbConfirmCancel(void* u);  // back to the script list
    static aether::ui::UiNode* renderScriptItem(
        aether::ui::NodeArena& a, int idx, bool focused, void* ud);

    static constexpr uint16_t kScriptItemH = 12;

    struct ScriptEntry { std::string path; std::string name; };

    std::vector<ScriptEntry>           scripts_;
    AppContext*                        ctx_      = nullptr;
    int                                selected_ = 0;
    State                              state_    = kMain;

    bool           running_     = false;
    badusb::Script parsedScript_;
    size_t         cmdIndex_    = 0;
    size_t         cmdTotal_    = 0;

    IUsbHid* hid_ = nullptr;

    aether::ui::ScrollState      scrollMain_;
    aether::ui::VirtualListState vlistScripts_;
    char scriptCountBuf_[16] = {};
    char runProgressBuf_[32] = {};
    char confirmBody_[72]    = {};
    char errorMsg_[64]       = {};
};

} // namespace nema
