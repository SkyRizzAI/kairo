#include "nema/screens/file_ops_modal.h"
#include "nema/runtime.h"
#include "nema/ui/widgets.h"
#include "nema/ui/view_dispatcher.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

FileOpsModal::FileOpsModal(Runtime& rt) : ComponentScreen(rt, 80) {}

void FileOpsModal::setup(const char* name, bool isDir, Callbacks cb) {
    std::snprintf(name_, sizeof(name_), "%s", name ? name : "");
    isDir_  = isDir;
    cb_     = cb;
}

void FileOpsModal::onResume() {
    st_ = St::Menu;
    scroll_.scrollMain   = 0;
    state_.focus.focused = 0;
    ComponentScreen::onResume();
}

// ── static button handlers ─────────────────────────────────────────────────

void FileOpsModal::sView(void* u) {
    auto* s = static_cast<FileOpsModal*>(u);
    if (s->cb_.onView) s->cb_.onView(s->cb_.user);
    s->rt_.view().goBack();
}
void FileOpsModal::sCopy(void* u) {
    auto* s = static_cast<FileOpsModal*>(u);
    if (s->cb_.onCopy) s->cb_.onCopy(s->cb_.user);
    s->rt_.view().goBack();
}
void FileOpsModal::sCut(void* u) {
    auto* s = static_cast<FileOpsModal*>(u);
    if (s->cb_.onCut) s->cb_.onCut(s->cb_.user);
    s->rt_.view().goBack();
}
void FileOpsModal::sPaste(void* u) {
    auto* s = static_cast<FileOpsModal*>(u);
    if (s->cb_.onPaste) s->cb_.onPaste(s->cb_.user);
    s->rt_.view().goBack();
}
void FileOpsModal::sRename(void* u) {
    auto* s = static_cast<FileOpsModal*>(u);
    if (s->cb_.onRename) s->cb_.onRename(s->cb_.user);
    s->rt_.view().goBack();
}
void FileOpsModal::sDelete(void* u) {
    auto* s = static_cast<FileOpsModal*>(u);
    std::snprintf(s->confirmBody_, sizeof(s->confirmBody_), "Delete \"%s\"?", s->name_);
    s->st_    = St::ConfirmDelete;
    s->dirty_ = true;
    s->requestRedraw();
}
void FileOpsModal::sDeleteConfirm(void* u) {
    auto* s = static_cast<FileOpsModal*>(u);
    if (s->cb_.onDelete) s->cb_.onDelete(s->cb_.user);
    s->rt_.view().goBack();
}
void FileOpsModal::sNewFolder(void* u) {
    auto* s = static_cast<FileOpsModal*>(u);
    if (s->cb_.onNewFolder) s->cb_.onNewFolder(s->cb_.user);
    s->rt_.view().goBack();
}
void FileOpsModal::sBack(void* u) {
    auto* s = static_cast<FileOpsModal*>(u);
    if (s->st_ == St::ConfirmDelete) {
        s->st_    = St::Menu;
        s->dirty_ = true;
        s->requestRedraw();
    } else {
        s->rt_.view().goBack();
    }
}

// ── build ──────────────────────────────────────────────────────────────────

aether::ui::UiNode* FileOpsModal::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    UiNode* list = ListContainer(a, scroll_, {});
    UiNode* prev = nullptr;

    auto row = [&](const char* label, void (*fn)(void*)) {
        ListEntry e; e.label = label; e.onPress = fn; e.user = this;
        UiNode* n = ListItemRow(a, e);
        if (!n) return;
        if (!prev) list->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };

    if (st_ == St::ConfirmDelete) {
        row("Delete", sDeleteConfirm);
        row("Cancel", sBack);
        return View(a, root, {
            ListSection(a, confirmBody_),
            list,
        });
    }

    if (!isDir_) row("View",   sView);
    if (cb_.onCopy)      row("Copy",       sCopy);
    if (cb_.onCut)       row("Cut",        sCut);
    if (cb_.onPaste)     row("Paste",      sPaste);
    if (cb_.onRename)    row("Rename",     sRename);
    if (cb_.onDelete)    row("Delete",     sDelete);
    if (cb_.onNewFolder) row("New Folder", sNewFolder);
    row("Cancel", sBack);

    return View(a, root, {
        ListSection(a, name_),
        list,
    });
}

} // namespace nema
