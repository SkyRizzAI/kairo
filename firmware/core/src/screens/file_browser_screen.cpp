#include "nema/screens/file_browser_screen.h"
#include "nema/runtime.h"
#include "nema/ui/widgets.h"
#include "nema/ui/icon_pack.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/input/input_code.h"
#include "nema/system/capabilities.h"
#include "nema/system/capability_registry.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace nema {

using namespace aether::ui;

FileBrowserScreen::FileBrowserScreen(Runtime& rt)
    : ComponentScreen(rt, 768), opsModal_(rt), viewer_(rt) {}

// ── helpers ───────────────────────────────────────────────────────────────────

static bool ciLess(const std::string& a, const std::string& b) {
    size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; i++) {
        unsigned char ca = (unsigned char)std::tolower((unsigned char)a[i]);
        unsigned char cb = (unsigned char)std::tolower((unsigned char)b[i]);
        if (ca != cb) return ca < cb;
    }
    return a.size() < b.size();
}

static void formatSize(char* buf, size_t n, uint32_t bytes) {
    if (bytes < 1024u)              std::snprintf(buf, n, "%uB",  (unsigned)bytes);
    else if (bytes < 1024u * 1024u) std::snprintf(buf, n, "%uK", (unsigned)(bytes / 1024u));
    else                            std::snprintf(buf, n, "%uM", (unsigned)(bytes / (1024u * 1024u)));
}

static std::string basename(const std::string& path) {
    size_t slash = path.find_last_of('/');
    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

// Smart path: if > 20 chars, show /…/parent/name (last 2 segments).
static void formatPathBuf(char* buf, size_t n, const std::string& path) {
    if (path.size() <= 20) {
        std::snprintf(buf, n, "%s", path.c_str());
        return;
    }
    size_t last = path.rfind('/');
    if (last == std::string::npos || last == 0) {
        std::snprintf(buf, n, "%s", path.c_str());
        return;
    }
    size_t prev = path.rfind('/', last - 1);
    if (prev != std::string::npos && prev > 0)
        std::snprintf(buf, n, "/...%s", path.c_str() + prev);
    else
        std::snprintf(buf, n, "/...%s", path.c_str() + last);
}

// Recursive copy: handles both files and directories.
static bool recursiveCopy(IFileSystem* fs, const std::string& src, const std::string& dst) {
    std::vector<FsEntry> children;
    if (fs->list(src, children)) {
        fs->mkdir(dst);
        for (const auto& c : children) {
            if (!recursiveCopy(fs, src + "/" + c.name, dst + "/" + c.name)) return false;
        }
        return true;
    }
    std::vector<uint8_t> data;
    return fs->read(src, data) && fs->write(dst, data.data(), data.size());
}

// ── directory model ─────────────────────────────────────────────────────────

void FileBrowserScreen::reload() {
    entries_.clear();
    if (IFileSystem* fs = rt_.fs()) fs->list(cwd_, entries_);

    std::sort(entries_.begin(), entries_.end(), [](const FsEntry& a, const FsEntry& b) {
        if (a.isDir != b.isDir) return a.isDir;
        return ciLess(a.name, b.name);
    });
    if (entries_.size() > kMaxEntries) entries_.resize(kMaxEntries);

    formatPathBuf(pathBuf_, sizeof(pathBuf_), cwd_);

    scroll_.scrollMain   = 0;
    state_.focus.focused = 0;
}

void FileBrowserScreen::goUp() {
    if (cwd_ == "/") return;
    size_t slash = cwd_.find_last_of('/');
    cwd_ = (slash == std::string::npos || slash == 0) ? "/" : cwd_.substr(0, slash);
    reload();
    dirty_ = true;
    requestRedraw();
}

void FileBrowserScreen::openEntry(int entryIndex) {
    if (entryIndex < 0) { goUp(); return; }
    if (entryIndex >= (int)entries_.size()) return;
    const FsEntry& e = entries_[(size_t)entryIndex];
    if (e.isDir) {
        cwd_ = (cwd_ == "/") ? ("/" + e.name) : (cwd_ + "/" + e.name);
        reload();
        dirty_ = true;
        requestRedraw();
    } else {
        std::string path = (cwd_ == "/") ? ("/" + e.name) : (cwd_ + "/" + e.name);
        viewer_.setPath(path.c_str());
        rt_.view().navigate(viewer_);
    }
}

void FileBrowserScreen::onRowPress(void* u) {
    auto* r = static_cast<Row*>(u);
    r->self->openEntry(r->entryIndex);
}

// ── lifecycle ─────────────────────────────────────────────────────────────────

void FileBrowserScreen::onResume() {
    PendingOp op = pendingOp_;
    pendingOp_   = PendingOp::None;

    if (op == PendingOp::StartRename) {
        startRenameKeyboard();
        dirty_ = true;
        requestRedraw();
        return;
    }
    if (op == PendingOp::StartNewFolder) {
        startNewFolderKeyboard();
        dirty_ = true;
        requestRedraw();
        return;
    }

    renaming_ = newFoldering_ = false;
    swallowCode_ = false;
    reload();
    ComponentScreen::onResume();
}

void FileBrowserScreen::tick(uint64_t nowMs) {
    ComponentScreen::tick(nowMs);
    if (pendingOp_ == PendingOp::View) {
        pendingOp_ = PendingOp::None;
        viewer_.setPath(pendingPath_.c_str());
        rt_.view().navigate(viewer_);
    }
}

bool FileBrowserScreen::onBackPressed() {
    if (renaming_ || newFoldering_) {
        renaming_ = newFoldering_ = false;
        dirty_    = true;
        requestRedraw();
        return true;
    }
    if (cwd_ != "/") { goUp(); return true; }
    return false;
}

// ── input ─────────────────────────────────────────────────────────────────────

void FileBrowserScreen::onAction(input::Action a) {
    if (renaming_ || newFoldering_) {
        if (!kbd_.linear) return;
        bool done = false, cancel = false;
        kbd_.handleAction(a, done, cancel);
        if (renaming_) applyRename(done, cancel);
        else           applyNewFolder(done, cancel);
        return;
    }
    if (a == input::Action::Menu) {
        showOpsMenu(state_.focus.focused);
        return;
    }
    ComponentScreen::onAction(a);
}

void FileBrowserScreen::onCode(input::Code c) {
    if (renaming_ || newFoldering_) {
        if (kbd_.linear) return;
        if (swallowCode_) { swallowCode_ = false; return; }
        bool done = false, cancel = false;
        kbd_.handle(input::keyFromCode(c), done, cancel);
        if (renaming_) applyRename(done, cancel);
        else           applyNewFolder(done, cancel);
        return;
    }
    ComponentScreen::onCode(c);
}

void FileBrowserScreen::draw(Canvas& c) {
    if (renaming_ || newFoldering_) { kbd_.draw(c, renamePrompt_); return; }
    ComponentScreen::draw(c);
}

// ── ops menu ──────────────────────────────────────────────────────────────────

void FileBrowserScreen::showOpsMenu(int focused) {
    bool showUp  = (cwd_ != "/");
    int  entryIdx = focused - (showUp ? 1 : 0);

    // Focused on ".." → directory-level menu (paste + new folder).
    if (entryIdx < 0) {
        showDirMenu();
        return;
    }
    if (entryIdx >= (int)entries_.size()) return;

    const FsEntry& e = entries_[(size_t)entryIdx];
    pendingName_  = e.name;
    pendingIsDir_ = e.isDir;
    pendingPath_  = (cwd_ == "/") ? ("/" + e.name) : (cwd_ + "/" + e.name);

    FileOpsModal::Callbacks cb;
    if (!e.isDir) cb.onView = cbView;
    cb.onCopy      = cbCopy;
    cb.onCut       = cbCut;
    cb.onPaste     = clipboard_.has ? cbPaste : nullptr;
    cb.onRename    = cbRename;
    cb.onDelete    = cbDelete;
    cb.onNewFolder = cbNewFolder;
    cb.user        = this;

    opsModal_.setup(e.name.c_str(), e.isDir, cb);
    rt_.view().navigate(opsModal_);
}

void FileBrowserScreen::showDirMenu() {
    std::string title = (cwd_ == "/") ? "/" : basename(cwd_);

    FileOpsModal::Callbacks cb;
    cb.onPaste     = clipboard_.has ? cbPaste : nullptr;
    cb.onNewFolder = cbNewFolder;
    cb.user        = this;

    opsModal_.setup(title.c_str(), true, cb);
    rt_.view().navigate(opsModal_);
}

// ── clipboard operations ──────────────────────────────────────────────────────

void FileBrowserScreen::doCopy() {
    clipboard_ = {pendingPath_, false, true};
}

void FileBrowserScreen::doCut() {
    clipboard_ = {pendingPath_, true, true};
}

void FileBrowserScreen::doPaste() {
    if (!clipboard_.has) return;
    IFileSystem* fs = rt_.fs();
    if (!fs) return;

    std::string dstName = basename(clipboard_.path);
    std::string dst = (cwd_ == "/") ? ("/" + dstName) : (cwd_ + "/" + dstName);

    if (recursiveCopy(fs, clipboard_.path, dst)) {
        if (clipboard_.isCut) {
            fs->removeAll(clipboard_.path);
            clipboard_.has = false;
        }
    }
}

void FileBrowserScreen::doDelete() {
    IFileSystem* fs = rt_.fs();
    if (fs) fs->removeAll(pendingPath_);
}

// ── rename ────────────────────────────────────────────────────────────────────

void FileBrowserScreen::startRenameKeyboard() {
    kbd_.clear();
    kbd_.linear = !rt_.capabilities().has(caps::Input2D);
    std::snprintf(renamePrompt_, sizeof(renamePrompt_), "Rename:");
    size_t len = pendingName_.size();
    if (len >= sizeof(kbd_.buf)) len = sizeof(kbd_.buf) - 1;
    std::memcpy(kbd_.buf, pendingName_.c_str(), len);
    kbd_.len = (uint8_t)len;
    renaming_    = true;
    swallowCode_ = true;
}

void FileBrowserScreen::applyRename(bool done, bool cancel) {
    if (!done && !cancel) { dirty_ = true; requestRedraw(); return; }
    renaming_ = false;
    if (done) {
        std::string newName(kbd_.buf, (size_t)kbd_.len);
        if (!newName.empty() && newName != pendingName_) {
            std::string dst = (cwd_ == "/") ? ("/" + newName) : (cwd_ + "/" + newName);
            if (IFileSystem* fs = rt_.fs()) fs->rename(pendingPath_, dst);
        }
    }
    reload();
    dirty_ = true;
    requestRedraw();
}

// ── new folder ────────────────────────────────────────────────────────────────

void FileBrowserScreen::startNewFolderKeyboard() {
    kbd_.clear();
    kbd_.linear = !rt_.capabilities().has(caps::Input2D);
    std::snprintf(renamePrompt_, sizeof(renamePrompt_), "New folder:");
    newFoldering_ = true;
    swallowCode_  = true;
}

void FileBrowserScreen::applyNewFolder(bool done, bool cancel) {
    if (!done && !cancel) { dirty_ = true; requestRedraw(); return; }
    newFoldering_ = false;
    if (done) {
        std::string name(kbd_.buf, (size_t)kbd_.len);
        if (!name.empty()) {
            std::string path = (cwd_ == "/") ? ("/" + name) : (cwd_ + "/" + name);
            if (IFileSystem* fs = rt_.fs()) fs->mkdir(path);
        }
    }
    reload();
    dirty_ = true;
    requestRedraw();
}

// ── FileOpsModal static callbacks ─────────────────────────────────────────────

void FileBrowserScreen::cbView(void* u) {
    auto* self = static_cast<FileBrowserScreen*>(u);
    self->pendingOp_ = PendingOp::View;
}
void FileBrowserScreen::cbCopy(void* u) {
    static_cast<FileBrowserScreen*>(u)->doCopy();
}
void FileBrowserScreen::cbCut(void* u) {
    static_cast<FileBrowserScreen*>(u)->doCut();
}
void FileBrowserScreen::cbPaste(void* u) {
    auto* self = static_cast<FileBrowserScreen*>(u);
    self->doPaste();
    self->reload();
    self->dirty_ = true;
    self->requestRedraw();
}
void FileBrowserScreen::cbRename(void* u) {
    auto* self = static_cast<FileBrowserScreen*>(u);
    self->pendingOp_ = PendingOp::StartRename;
}
void FileBrowserScreen::cbDelete(void* u) {
    static_cast<FileBrowserScreen*>(u)->doDelete();
}
void FileBrowserScreen::cbNewFolder(void* u) {
    auto* self = static_cast<FileBrowserScreen*>(u);
    self->pendingOp_ = PendingOp::StartNewFolder;
}

// ── view ─────────────────────────────────────────────────────────────────────

aether::ui::UiNode* FileBrowserScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    const bool   showUp = (cwd_ != "/");
    const size_t total  = entries_.size() + (showUp ? 1 : 0);
    rows_.resize(total);
    accStrs_.resize(entries_.size());

    const IconDef* folderIco = findIcon("file.folder");
    const IconDef* fileIco   = findIcon("file.file");

    UiNode* list = ListContainer(a, scroll_, {});
    UiNode* prev = nullptr;
    size_t  ri   = 0;

    char sizeBuf[16];

    auto addRow = [&](const IconDef* ico, const char* name,
                      const char* val, bool chevron, int entryIndex) -> bool {
        rows_[ri] = { this, entryIndex };

        ListEntry e;
        e.label   = name;
        e.value   = val;
        e.chevron = chevron;
        if (ico) { e.leftIcon = ico->bitmap; e.iconW = ico->w; e.iconH = ico->h; }
        e.onPress = onRowPress;
        e.user    = &rows_[ri];

        UiNode* row = ListItemRow(a, e);
        if (!row) return false;

        if (!prev) list->firstChild = row; else prev->nextSibling = row;
        prev = row;
        ri++;
        return true;
    };

    if (showUp) addRow(folderIco, "..", nullptr, false, -1);

    for (size_t i = 0; i < entries_.size(); i++) {
        const FsEntry& e = entries_[i];
        const char* val = nullptr;
        if (!e.isDir) {
            formatSize(sizeBuf, sizeof(sizeBuf), e.size);
            accStrs_[i] = sizeBuf;
            val = accStrs_[i].c_str();
        }
        if (!addRow(e.isDir ? folderIco : fileIco, e.name.c_str(),
                    val, e.isDir, (int)i))
            break;
    }

    if (total == 0) {
        ListEntry e; e.label = "(empty)";
        list->firstChild = ListItemRow(a, e);
    }

    return View(a, root, {
        ListSection(a, pathBuf_),
        list,
    });
}

} // namespace nema
