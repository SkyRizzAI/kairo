#pragma once
// File Browser — Flipper-style VFS file manager (scrolling icon list).
//
// Activate on a folder → navigate into it.
// Activate on a file   → open raw text viewer.
// Action::Menu (hold select) → context menu: View / Copy / Cut / Paste /
//   Rename / Delete. Paste is shown only when the in-memory clipboard has
//   content. Delete shows an inline confirmation before executing.
// Back → go up one level, exit to Home at root.
#include "nema/ui/component_screen.h"
#include "nema/ui/virtual_keyboard.h"
#include "nema/ui/node.h"
#include "nema/hal/filesystem.h"
#include "nema/screens/file_ops_modal.h"
#include "nema/screens/file_text_viewer_screen.h"
#include <vector>
#include <string>

namespace nema {

class Runtime;

class FileBrowserScreen : public ComponentScreen {
public:
    explicit FileBrowserScreen(Runtime& rt);

    void        onResume()      override;
    bool        onBackPressed() override;
    void        onAction(input::Action a) override;
    void        onCode(input::Code c)     override;
    void        draw(Canvas& c)           override;
    void        tick(uint64_t nowMs)      override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    struct Row { FileBrowserScreen* self; int entryIndex; };

    static constexpr size_t kMaxEntries = 128;

    // ── directory navigation ──────────────────────────────────────────────
    void reload();
    void openEntry(int entryIndex);
    void goUp();
    void showOpsMenu(int focused);

    static void onRowPress(void* u);

    // ── clipboard ─────────────────────────────────────────────────────────
    struct Clipboard { std::string path; bool isCut = false; bool has = false; };
    Clipboard clipboard_;

    void doCopy();
    void doCut();
    void doPaste();
    void doDelete();

    // ── rename (VirtualKeyboard overlay) ─────────────────────────────────
    void startRenameKeyboard();
    void applyRename(bool done, bool cancel);

    aether::ui::VirtualKeyboard kbd_;
    bool  renaming_    = false;
    bool  swallowCode_ = false;
    char  renamePrompt_[64] = {};

    // ── deferred ops (set before goBack on modal, consumed in onResume/tick)
    enum class PendingOp { None, View, StartRename };
    PendingOp   pendingOp_  = PendingOp::None;
    std::string pendingPath_;
    std::string pendingName_;
    bool        pendingIsDir_ = false;

    // ── FileOpsModal callbacks ────────────────────────────────────────────
    static void cbView  (void* u);
    static void cbCopy  (void* u);
    static void cbCut   (void* u);
    static void cbPaste (void* u);
    static void cbRename(void* u);
    static void cbDelete(void* u);

    // ── sub-screens (owned, stable addresses) ────────────────────────────
    FileOpsModal      opsModal_;
    TextViewerScreen  viewer_;

    // ── model ─────────────────────────────────────────────────────────────
    std::string              cwd_ = "/";
    std::vector<FsEntry>     entries_;
    std::vector<Row>         rows_;
    std::vector<std::string> accStrs_;
    aether::ui::ScrollState  scroll_;
    char                     pathBuf_[128] = "/";
};

} // namespace nema
