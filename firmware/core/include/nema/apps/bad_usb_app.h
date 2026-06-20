#pragma once
#include "nema/app/component_app.h"
#include "nema/apps/badusb_parser.h"
#include <string>
#include <vector>

namespace nema {

struct IFileSystem;
struct IUsbHid;

class BadUsbApp : public ComponentApp {
public:
    const char* id()   const override { return "com.palanu.badusb"; }
    const char* name() const override { return "BadUSB"; }

    bool fullscreen()      const override { return true; }
    bool capturesInput()   const override { return true; }

protected:
    void onStart(AppContext& ctx) override;
    aether::ui::UiNode* build(aether::ui::NodeArena& arena, AppContext& ctx) override;
    bool onKey(Key k, AppContext& ctx) override;
    uint32_t tickIntervalMs() const override { return running_ ? 50u : 0u; }
    bool onTick(AppContext& ctx) override;
    size_t arenaCapacity() const override { return 512; }

private:
    void scanScripts(AppContext& ctx);
    void startExecution(AppContext& ctx);
    void execNextCommand();

    struct ScriptEntry { std::string path; std::string name; };
    std::vector<ScriptEntry> scripts_;
    int selected_ = 0;

    bool running_ = false;
    badusb::Script parsedScript_;
    size_t cmdIndex_ = 0;
    size_t cmdTotal_ = 0;

    IFileSystem* fs_ = nullptr;
    IUsbHid* hid_ = nullptr;
};

} // namespace nema
