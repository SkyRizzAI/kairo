#pragma once
#include "nema/app/component_app.h"
#include "nema/apps/badusb_parser.h"
#include <string>
#include <vector>

namespace nema {

class  Runtime;
struct IFileSystem;
struct IUsbHid;

// BadUSB BadUSB script executor — runs as a proper app on its own thread
// (Plan 84 Fase 3). Each launch gets a fresh onStart(); execution state is
// confined to the app thread so there are no GUI-thread data races.
class BadUsbApp : public ComponentApp {
public:
    const char* id()       const override { return "com.palanu.badusb"; }
    const char* name()     const override { return "BadUSB"; }
    const char* category() const override { return "System"; }
    bool fullscreen()      const override { return true; }

protected:
    void onStart(AppContext& ctx) override;
    aether::ui::UiNode* build(aether::ui::NodeArena& arena, AppContext& ctx) override;
    bool onKey(Key k, AppContext& ctx) override;
    bool onTick(AppContext& ctx) override;
    uint32_t tickIntervalMs() const override { return running_ ? 20u : 0u; }
    bool capturesInput() const override { return true; }

private:
    void scanScripts(IFileSystem* fs);
    void startExecution(IFileSystem* fs, Runtime& rt);
    void execNextCommand();

    struct ScriptEntry { std::string path; std::string name; };
    std::vector<ScriptEntry> scripts_;
    int selected_ = 0;

    bool running_ = false;
    badusb::Script parsedScript_;
    size_t cmdIndex_ = 0;
    size_t cmdTotal_ = 0;

    IUsbHid* hid_ = nullptr;
};

} // namespace nema
