#pragma once
#include "nema/ui/component_screen.h"
#include "nema/apps/badusb_parser.h"
#include <string>
#include <vector>

namespace nema {

struct IFileSystem;
struct IUsbHid;

class BadUsbApp : public ComponentScreen {
public:
    explicit BadUsbApp(Runtime& rt) : ComponentScreen(rt) {}

    bool fullscreen() const override { return true; }

    void onResume() override;
    void tick(uint64_t nowMs) override;
    void onAction(input::Action a) override;
    aether::ui::UiNode* build(aether::ui::NodeArena& arena, Runtime& rt) override;

private:
    void scanScripts();
    void startExecution();
    void execNextCommand();

    struct ScriptEntry { std::string path; std::string name; };
    std::vector<ScriptEntry> scripts_;
    int selected_ = 0;

    bool running_ = false;
    badusb::Script parsedScript_;
    size_t cmdIndex_ = 0;
    size_t cmdTotal_ = 0;

    IFileSystem* fs_  = nullptr;
    IUsbHid*    hid_  = nullptr;
};

} // namespace nema
