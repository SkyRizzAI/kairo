#pragma once
// File Browser — Flipper-style VFS file manager (scrolling icon list).
//
// Activate on a folder → navigate into it.
// Activate on a file   → open raw text viewer.
// Action::Menu (hold select) → context menu: View / Copy / Cut / Paste /
//   Rename / Delete / New Folder. On ".." → dir menu: Paste / New Folder.
// Back → go up one level, exit to Home at root.
#include "nema/ui/component_screen.h"
#include "nema/ui/virtual_keyboard.h"
#include "nema/ui/virtual_list.h"
#include "nema/ui/node.h"
#include "nema/hal/filesystem.h"
#include "nema/screens/file_ops_modal.h"
#include "nema/screens/file_text_viewer_screen.h"
#include "nema/screens/confirm_modal.h"
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
    static constexpr size_t   kMaxEntries = 128;
    static constexpr uint16_t kItemH      = 12;

    // ── directory navigation ──────────────────────────────────────────────
    void reload();          // async: lists cwd_ behind a "Loading…" busy overlay
    void finishReload();    // UI-thread: sorts/format staging_ → entries_, fixes focus
    void openEntry(int entryIndex);
    void goUp();
    void showOpsMenu(int focused);
    void showDirMenu();           // for ".." row — paste + new folder

    static aether::ui::UiNode* renderItem(aether::ui::NodeArena& a,
                                          int idx, bool focused, void* ud);

    // ── clipboard ─────────────────────────────────────────────────────────
    struct Clipboard { std::string path; bool isCut = false; bool has = false; };
    Clipboard clipboard_;

    void doCopy();
    void doCut();
    void handlePaste();           // resolve dst, confirm overwrite, then doPasteRun()
    void doPasteRun();            // runBusy("Copying…") copy (+remove on cut) → reload
    void handleDelete();          // runBusy("Deleting…") removeAll → reload
    void doRenameRun();           // fs->rename → reload

    // ── keyboard overlay (rename + new folder) ────────────────────────────
    void startRenameKeyboard();
    void applyRename(bool done, bool cancel);
    void startNewFolderKeyboard();
    void applyNewFolder(bool done, bool cancel);

    aether::ui::VirtualKeyboard kbd_;
    bool  renaming_     = false;
    bool  newFoldering_ = false;
    bool  swallowCode_  = false;
    char  renamePrompt_[64] = {};

    // ── deferred ops (set before goBack on modal, consumed in onResume/tick)
    enum class PendingOp { None, View, StartRename, StartNewFolder, Paste, Delete };
    PendingOp   pendingOp_  = PendingOp::None;
    std::string pendingPath_;
    std::string pendingName_;
    bool        pendingIsDir_ = false;
    std::string pasteDst_;        // destination path for a pending paste/overwrite
    std::string renameDst_;       // destination path for a pending rename/overwrite

    // ── FileOpsModal callbacks ────────────────────────────────────────────
    static void cbView     (void* u);
    static void cbCopy     (void* u);
    static void cbCut      (void* u);
    static void cbPaste    (void* u);
    static void cbRename   (void* u);
    static void cbDelete   (void* u);
    static void cbNewFolder(void* u);

    // ── overwrite-confirm thunks (ConfirmModal::onConfirm) ────────────────
    static void doPasteConfirm (void* u);
    static void doRenameConfirm(void* u);

    // ── sub-screens (owned, stable addresses) ────────────────────────────
    FileOpsModal      opsModal_;
    TextViewerScreen  viewer_;
    ConfirmModal      confirm_;

    // ── model ─────────────────────────────────────────────────────────────
    std::string                  cwd_ = "/";
    std::string                  focusCwd_;   // dir the list focus was last reset for
    std::vector<FsEntry>         entries_;
    std::vector<FsEntry>         staging_;    // async list result (worker thread → finishReload)
    std::vector<std::string>     accStrs_;   // formatted file sizes, indexed with entries_
    aether::ui::VirtualListState vlist_;
    char                         pathBuf_[128] = "/";
};

} // namespace nema
