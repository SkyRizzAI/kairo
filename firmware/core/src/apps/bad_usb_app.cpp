#include "nema/apps/bad_usb_app.h"
#include "nema/hal/filesystem.h"
#include "nema/hal/usb_hid.h"
#include "nema/ui/widgets.h"
#include "nema/ui/style_tokens.h"
#include "nema/app/app_context.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/service/service_container.h"
#include <cstdio>
#include <cstring>

namespace nema {

using namespace aether::ui;

void BadUsbApp::onStart(AppContext& ctx) {
    hid_     = ctx.runtime().container().resolve<IUsbHid>();
    running_  = false;
    selected_ = 0;
    scanScripts(ctx.runtime().fs());
}

void BadUsbApp::scanScripts(IFileSystem* fs) {
    scripts_.clear();
    if (!fs) return;
    std::vector<FsEntry> entries;
    if (!fs->list("/system/data/com.palanu.badusb", entries)) return;
    for (auto& e : entries) {
        if (e.isDir) continue;
        std::string nm = e.name;
        if (nm.size() > 3 && nm.substr(nm.size() - 3) == ".dd")
            scripts_.push_back({"/system/data/com.palanu.badusb/" + e.name, nm});
    }
}

bool BadUsbApp::onTick(AppContext& ctx) {
    (void)ctx;
    if (running_ && hid_) {
        execNextCommand();
        return true;
    }
    return false;
}

bool BadUsbApp::onKey(Key k, AppContext& ctx) {
    if (running_) {
        if (k == Key::Cancel) {
            running_ = false;
            if (hid_) hid_->releaseAll();
            return true;
        }
        return false;
    }
    switch (k) {
        case Key::Up:
        case Key::Left:
            if (selected_ > 0) { selected_--; return true; }
            return false;
        case Key::Down:
        case Key::Right:
            if (selected_ < (int)scripts_.size() - 1) { selected_++; return true; }
            return false;
        case Key::Select:
            if (!scripts_.empty() && hid_) {
                startExecution(ctx.runtime().fs(), ctx.runtime());
                return true;
            }
            return false;
        case Key::Cancel:
            return false;  // base handles: requestExit()
        default:
            return false;
    }
}

aether::ui::UiNode* BadUsbApp::build(aether::ui::NodeArena& arena, AppContext&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1;
    root.padding = aether::theme().space.sm; root.gap = aether::theme().space.sm;
    root.align = Align::Stretch;
    Style menu; menu.dir = FlexDir::Col; menu.align = Align::Stretch; menu.gap = 1;

    if (running_) {
        char prog[48];
        std::snprintf(prog, sizeof(prog), "%zu/%zu", cmdIndex_ + 1, cmdTotal_);
        return View(arena, root, {
            TitleBar(arena, "BadUSB"),
            SmartLabel(arena, "Running..."),
            SmartLabel(arena, prog),
            SmartLabel(arena, "Back: stop"),
        });
    }

    if (scripts_.empty()) {
        return View(arena, root, {
            TitleBar(arena, "BadUSB"),
            SmartLabel(arena, "No scripts in /badusb/"),
            SmartLabel(arena, "Upload .dd via Forge"),
        });
    }

    char count[32];
    std::snprintf(count, sizeof(count), "%d/%d",
                  selected_ + 1, (int)scripts_.size());
    const char* selectedName = scripts_[(size_t)selected_].name.c_str();

    if (!hid_ || !hid_->isReady()) {
        return View(arena, root, {
            TitleBar(arena, "BadUSB"),
            SmartLabel(arena, "Script:"),
            SmartLabel(arena, selectedName),
            SmartLabel(arena, count),
            SmartLabel(arena, "USB HID not enabled"),
            SmartLabel(arena, "(TinyUSB mode required)"),
        });
    }

    return View(arena, root, {
        TitleBar(arena, "BadUSB"),
        Col(arena, menu, {
            SmartLabel(arena, "Script:"),
            SmartLabel(arena, selectedName),
            SmartLabel(arena, count),
            SmartLabel(arena, "OK: run   Up/Dn: select"),
        }),
    });
}

void BadUsbApp::startExecution(IFileSystem* fs, Runtime& rt) {
    if (scripts_.empty() || !fs) return;
    std::vector<uint8_t> data;
    if (!fs->read(scripts_[selected_].path, data)) return;
    parsedScript_ = badusb::parse((const char*)data.data(), data.size());
    cmdIndex_ = 0;
    cmdTotal_ = parsedScript_.size();
    running_ = (cmdTotal_ > 0);
    rt.log().info("BadUsbApp", "executing",
        {{"script", scripts_[selected_].name},
         {"cmds", std::to_string(cmdTotal_)}});
}

void BadUsbApp::execNextCommand() {
    while (cmdIndex_ < cmdTotal_) {
        auto& cmd = parsedScript_[cmdIndex_];
        cmdIndex_++;
        switch (cmd.type) {
            case badusb::Command::Key:
                hid_->sendKey(cmd.modifier, cmd.keycode);
                break;
            case badusb::Command::String:
                hid_->sendString(cmd.text.c_str(), cmd.delayMs);
                break;
            case badusb::Command::Delay:
                hid_->delay(cmd.delayMs);
                break;
            default: break;
        }
    }
    running_ = false;
    hid_->releaseAll();
}

} // namespace nema
