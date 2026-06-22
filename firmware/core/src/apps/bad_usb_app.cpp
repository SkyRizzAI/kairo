#include "nema/apps/bad_usb_app.h"
#include "nema/hal/filesystem.h"
#include "nema/hal/usb_hid.h"
#include "nema/ui/widgets.h"
#include "nema/app/app_context.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/service/service_container.h"
#include <cstdio>
#include <cstring>

namespace nema {

using namespace aether::ui;

void BadUsbApp::onStart(AppContext& ctx) {
    hid_      = ctx.runtime().container().resolve<IUsbHid>();
    running_  = false;
    selected_ = 0;
    state_        = kMain;
    suppressNext_ = true;
    errorMsg_[0]  = '\0';
    scrollMain_.scrollMain    = 0;
    scrollScripts_.scrollMain = 0;
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
        if (!running_) state_ = kScriptList;
        return true;
    }
    return false;
}

bool BadUsbApp::onKey(Key k, AppContext& ctx) {
    (void)ctx;
    if (running_) {
        if (k == Key::Cancel) {
            running_ = false;
            if (hid_) hid_->releaseAll();
            state_ = kScriptList;
        }
        return true;  // swallow all input while running
    }
    if (state_ == kScriptList && k == Key::Cancel) { state_ = kMain;       return true; }
    if (state_ == kError      && k == Key::Cancel) { state_ = kScriptList; return true; }
    return false;
}

void BadUsbApp::cbRunScript(void* u) {
    auto* self = static_cast<BadUsbApp*>(u);
    if (self->suppressNext_) { self->suppressNext_ = false; return; }
    self->state_ = kScriptList;
    self->scrollScripts_.scrollMain = 0;
}

void BadUsbApp::cbSelectScript(void* u) {
    auto* r    = static_cast<ScriptRow*>(u);
    auto* self = r->self;
    self->selected_ = r->index;

    if (!self->hid_ || !self->hid_->isReady()) {
        std::snprintf(self->errorMsg_, sizeof(self->errorMsg_),
                      "USB HID not enabled.\nEnable TinyUSB mode in\nCMakeLists.txt");
        self->state_ = kError;
        return;
    }

    auto& rt = self->ctx_->runtime();
    self->startExecution(rt.fs(), rt);
    if (self->running_) {
        self->state_ = kRunning;
    } else {
        std::snprintf(self->errorMsg_, sizeof(self->errorMsg_), "Failed to load script");
        self->state_ = kError;
    }
}

aether::ui::UiNode* BadUsbApp::build(NodeArena& arena, AppContext& ctx) {
    ctx_ = &ctx;
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    auto info = [&](const char* label, const char* value) {
        ListEntry e; e.label = label; e.value = value;
        return ListItemRow(arena, e);
    };

    // ── Error ────────────────────────────────────────────────────────────────
    if (state_ == kError) {
        return View(arena, root, {
            ListContainer(arena, scrollMain_, {
                ListSection(arena, "Cannot Run"),
                info("Reason", errorMsg_),
                info("Back",   "cancel"),
            }),
        });
    }

    // ── Running ──────────────────────────────────────────────────────────────
    if (state_ == kRunning) {
        const char* scriptName = (selected_ >= 0 && selected_ < (int)scripts_.size())
            ? scripts_[(size_t)selected_].name.c_str() : "?";
        std::snprintf(runProgressBuf_, sizeof(runProgressBuf_),
                      "%zu cmds", cmdTotal_);
        return View(arena, root, {
            ListContainer(arena, scrollMain_, {
                ListSection(arena, "Injecting"),
                info("Script",   scriptName),
                info("Commands", runProgressBuf_),
                info("Back",     "cancel"),
            }),
        });
    }

    // ── Script list ──────────────────────────────────────────────────────────
    if (state_ == kScriptList) {
        scriptRows_.clear();
        for (size_t i = 0; i < scripts_.size(); i++)
            scriptRows_.push_back({this, (int)i});

        UiNode* list = ListContainer(arena, scrollScripts_, {});
        UiNode* prev = nullptr;

        auto addRow = [&](UiNode* row) {
            if (!row) return;
            if (!prev) list->firstChild = row; else prev->nextSibling = row;
            prev = row;
        };

        if (scripts_.empty()) {
            ListEntry e; e.label = "(empty)";
            addRow(ListItemRow(arena, e));
        } else {
            for (size_t i = 0; i < scripts_.size(); i++) {
                ListEntry e;
                e.label   = scripts_[i].name.c_str();
                e.chevron = true;
                e.onPress = cbSelectScript;   // HID check is inside cbSelectScript
                e.user    = &scriptRows_[i];
                addRow(ListItemRow(arena, e));
            }
        }

        return View(arena, root, { list });
    }

    // ── Main ─────────────────────────────────────────────────────────────────
    bool hidReady = hid_ && hid_->isReady();
    std::snprintf(scriptCountBuf_, sizeof(scriptCountBuf_), "%d", (int)scripts_.size());

    ListEntry runRow;
    runRow.label   = "Run Script";
    runRow.chevron = true;
    runRow.onPress = cbRunScript;
    runRow.user    = this;

    return View(arena, root, {
        ListContainer(arena, scrollMain_, {
            ListItemRow(arena, runRow),
            ListSection(arena, "Info"),
            info("HID",     hidReady ? "Enabled" : "Disabled"),
            info("Scripts", scriptCountBuf_),
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
    running_  = (cmdTotal_ > 0);
    rt.log().info("BadUsbApp", "executing",
        {{"script", scripts_[selected_].name},
         {"cmds",   std::to_string(cmdTotal_)}});
}

void BadUsbApp::execNextCommand() {
    while (cmdIndex_ < cmdTotal_) {
        auto& cmd = parsedScript_[cmdIndex_];
        cmdIndex_++;
        switch (cmd.type) {
            case badusb::Command::Key:    hid_->sendKey(cmd.modifier, cmd.keycode);         break;
            case badusb::Command::String: hid_->sendString(cmd.text.c_str(), cmd.delayMs); break;
            case badusb::Command::Delay:  hid_->delay(cmd.delayMs);                        break;
            default: break;
        }
    }
    running_ = false;
    hid_->releaseAll();
}

} // namespace nema
