#include "nema/screens/file_browser_screen.h"
#include "nema/runtime.h"
#include "nema/ui/widgets.h"
#include "nema/ui/virtual_list.h"
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
    : ComponentScreen(rt, 768), opsModal_(rt), viewer_(rt), confirm_(rt) {}

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

// Breadcrumb: "/ > docs > notes". Segments after the first two are replaced
// with "..." to keep the header short ("/ > ... > parent > name").
static void formatPathBuf(char* buf, size_t n, const std::string& path) {
    if (path == "/") { std::snprintf(buf, n, "/"); return; }

    // Split into non-empty segments.
    std::vector<std::string> segs;
    size_t pos = 1;
    while (pos < path.size()) {
        size_t slash = path.find('/', pos);
        if (slash == std::string::npos) slash = path.size();
        segs.push_back(path.substr(pos, slash - pos));
        pos = slash + 1;
    }

    // Cap individual segment names to prevent overflow on narrow displays (~20 chars at Bold8).
    auto abbr = [](const std::string& s) -> std::string {
        return s.size() > 8 ? s.substr(0, 7) + "~" : s;
    };

    // Build "/ > seg1 > ... > segN" (truncate middle if > 3 segments).
    char tmp[128] = "/";
    size_t written = 1;
    if (segs.size() <= 3) {
        for (auto& s : segs) {
            int r = std::snprintf(tmp + written, sizeof(tmp) - written, " > %s", abbr(s).c_str());
            if (r > 0) written += (size_t)r;
        }
    } else {
        // More than 3 segments: show first + "..." + last two.
        int r = std::snprintf(tmp + written, sizeof(tmp) - written, " > %s > ...", abbr(segs[0]).c_str());
        if (r > 0) written += (size_t)r;
        for (size_t i = segs.size() - 2; i < segs.size(); i++) {
            r = std::snprintf(tmp + written, sizeof(tmp) - written, " > %s", abbr(segs[i]).c_str());
            if (r > 0) written += (size_t)r;
        }
    }

    // Final safety: if still too long for the 128px display, show only the leaf.
    if (written > 22) {
        std::snprintf(buf, n, "... > %s", abbr(segs.back()).c_str());
    } else {
        std::snprintf(buf, n, "%s", tmp);
    }
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
    // The directory listing can be slow on real storage (LittleFS / SD), so it
    // runs on the task worker behind a busy overlay. We list into staging_ (not
    // entries_) so build() keeps rendering the current, stable list until the
    // load finishes and finishReload() swaps it in on the UI thread.
    IFileSystem* fs  = rt_.fs();
    std::string  dir = cwd_;
    runBusy("Loading…",
            [this, fs, dir] {
                staging_.clear();
                if (fs) fs->list(dir, staging_);
            },
            [this] { finishReload(); });
}

void FileBrowserScreen::finishReload() {
    entries_ = std::move(staging_);
    staging_.clear();

    std::sort(entries_.begin(), entries_.end(), [](const FsEntry& a, const FsEntry& b) {
        if (a.isDir != b.isDir) return a.isDir;
        return ciLess(a.name, b.name);
    });
    if (entries_.size() > kMaxEntries) entries_.resize(kMaxEntries);

    formatPathBuf(pathBuf_, sizeof(pathBuf_), cwd_);

    // Pre-format file sizes so renderItem() can point to stable strings.
    accStrs_.resize(entries_.size());
    char sizeBuf[16];
    for (size_t i = 0; i < entries_.size(); i++) {
        if (!entries_[i].isDir) {
            formatSize(sizeBuf, sizeof(sizeBuf), entries_[i].size);
            accStrs_[i] = sizeBuf;
        } else {
            accStrs_[i].clear();
        }
    }

    // Focus handling: only reset to the top when the directory actually changed.
    // Re-entering the same directory after a file op (rename/delete/paste/view)
    // preserves the user's position; we just clamp it in case the count shrank.
    vlist_.totalCount = (int)entries_.size() + (cwd_ != "/" ? 1 : 0);
    if (cwd_ != focusCwd_) {
        vlist_.scrollMain   = 0;
        vlist_.focusedIndex = 0;
        focusCwd_           = cwd_;
    } else {
        vlist_.clampFocus();
        vlist_.scrollToFocused();
    }
    markDirty();
}

void FileBrowserScreen::goUp() {
    if (cwd_ == "/") return;
    size_t slash = cwd_.find_last_of('/');
    cwd_ = (slash == std::string::npos || slash == 0) ? "/" : cwd_.substr(0, slash);
    reload();
}

void FileBrowserScreen::openEntry(int entryIndex) {
    if (entryIndex < 0) { goUp(); return; }
    if (entryIndex >= (int)entries_.size()) return;
    const FsEntry& e = entries_[(size_t)entryIndex];
    if (e.isDir) {
        cwd_ = (cwd_ == "/") ? ("/" + e.name) : (cwd_ + "/" + e.name);
        reload();
    } else {
        std::string path = (cwd_ == "/") ? ("/" + e.name) : (cwd_ + "/" + e.name);
        viewer_.setPath(path.c_str());
        rt_.view().navigate(viewer_);
    }
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

    // View: open the selected file in the text viewer. (Handled here, not in
    // tick(), because goBack() from the ops modal calls onResume() synchronously
    // and would otherwise clear pendingOp_ before tick() ever saw it.)
    if (op == PendingOp::View) {
        viewer_.setPath(pendingPath_.c_str());
        rt_.view().navigate(viewer_);
        return;
    }
    // Paste / Delete were deferred from the ops modal so their busy overlay
    // (and any overwrite confirm) runs on this screen, now that it is active.
    if (op == PendingOp::Paste)  { handlePaste();  return; }
    if (op == PendingOp::Delete) { handleDelete(); return; }

    reload();
    ComponentScreen::onResume();
}

void FileBrowserScreen::tick(uint64_t nowMs) {
    ComponentScreen::tick(nowMs);
    // VirtualList items are focusable=false → state_.focus.count stays 0 →
    // ComponentScreen::tick's marquee guard never fires. Drive it here instead.
    if (!entries_.empty() && !renaming_ && !newFoldering_
            && (nowMs - lastMarqueeMs_) >= 66) {
        lastMarqueeMs_ = nowMs;
        requestRedraw();
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

    const bool showUp = (cwd_ != "/");
    const int  total  = (int)entries_.size() + (showUp ? 1 : 0);

    switch (a) {
    case input::Action::Prev:
    case input::Action::AdjustDown:
        if (vlist_.moveFocus(-1)) { dirty_ = true; requestRedraw(); }
        break;
    case input::Action::Next:
    case input::Action::AdjustUp:
        if (vlist_.moveFocus(+1)) { dirty_ = true; requestRedraw(); }
        break;
    case input::Action::Activate: {
        int fi = vlist_.focusedIndex;
        openEntry(showUp && fi == 0 ? -1 : fi - (showUp ? 1 : 0));
        break;
    }
    case input::Action::Menu:
        showOpsMenu(vlist_.focusedIndex);
        break;
    case input::Action::Back:
        if (!onBackPressed()) rt_.view().goBack();
        break;
    default:
        break;
    }
    (void)total;
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

void FileBrowserScreen::handlePaste() {
    IFileSystem* fs = rt_.fs();
    if (!clipboard_.has || !fs) { reload(); return; }

    std::string dstName = basename(clipboard_.path);
    pasteDst_ = (cwd_ == "/") ? ("/" + dstName) : (cwd_ + "/" + dstName);

    // Refuse to overwrite a destination without asking first.
    if (fs->exists(pasteDst_)) {
        char body[80];
        std::snprintf(body, sizeof(body), "Overwrite \"%s\"?", dstName.c_str());
        confirm_.setup("Overwrite", body, "Overwrite", doPasteConfirm, this, /*danger=*/true);
        rt_.view().push(confirm_);
        return;
    }
    doPasteRun();
}

void FileBrowserScreen::doPasteRun() {
    IFileSystem*      fs  = rt_.fs();
    Clipboard         cb  = clipboard_;
    const std::string dst = pasteDst_;
    runBusy("Copying…",
            [fs, cb, dst] {
                if (fs && recursiveCopy(fs, cb.path, dst)) {
                    if (cb.isCut) fs->removeAll(cb.path);
                }
            },
            [this, cb] {
                if (cb.isCut) clipboard_.has = false;
                reload();   // re-list the (now changed) directory
            });
}

void FileBrowserScreen::handleDelete() {
    IFileSystem*      fs   = rt_.fs();
    const std::string path = pendingPath_;
    runBusy("Deleting…",
            [fs, path] { if (fs) fs->removeAll(path); },
            [this] { reload(); });
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
            renameDst_ = (cwd_ == "/") ? ("/" + newName) : (cwd_ + "/" + newName);
            IFileSystem* fs = rt_.fs();
            if (fs && fs->exists(renameDst_)) {
                char body[80];
                std::snprintf(body, sizeof(body), "Overwrite \"%s\"?", newName.c_str());
                confirm_.setup("Overwrite", body, "Overwrite", doRenameConfirm, this, /*danger=*/true);
                rt_.view().push(confirm_);
                return;   // reload happens after the rename runs
            }
            doRenameRun();
            return;
        }
    }
    reload();
}

void FileBrowserScreen::doRenameRun() {
    if (IFileSystem* fs = rt_.fs()) fs->rename(pendingPath_, renameDst_);
    reload();
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
    // Deferred: the ops modal pops next, then onResume() runs handlePaste() so
    // the "Copying…" overlay (and any overwrite confirm) lives on this screen.
    static_cast<FileBrowserScreen*>(u)->pendingOp_ = PendingOp::Paste;
}
void FileBrowserScreen::cbRename(void* u) {
    auto* self = static_cast<FileBrowserScreen*>(u);
    self->pendingOp_ = PendingOp::StartRename;
}
void FileBrowserScreen::cbDelete(void* u) {
    // Deferred (see cbPaste): handled by handleDelete() in onResume().
    static_cast<FileBrowserScreen*>(u)->pendingOp_ = PendingOp::Delete;
}
void FileBrowserScreen::cbNewFolder(void* u) {
    auto* self = static_cast<FileBrowserScreen*>(u);
    self->pendingOp_ = PendingOp::StartNewFolder;
}

// ── overwrite-confirm thunks ──────────────────────────────────────────────────
// Start the busy op BEFORE popping the confirm modal: goBack() synchronously
// calls onResume() → reload(), and runBusy() ignores re-entry while busy, so the
// resume's stray reload is harmlessly dropped and our op's own reload wins.
void FileBrowserScreen::doPasteConfirm(void* u) {
    auto* self = static_cast<FileBrowserScreen*>(u);
    self->doPasteRun();
    self->rt_.view().goBack();
}
void FileBrowserScreen::doRenameConfirm(void* u) {
    auto* self = static_cast<FileBrowserScreen*>(u);
    self->doRenameRun();
    self->rt_.view().goBack();
}

// ── VirtualList renderItem callback ───────────────────────────────────────────

aether::ui::UiNode* FileBrowserScreen::renderItem(NodeArena& a, int idx,
                                                   bool focused, void* ud) {
    auto* self = static_cast<FileBrowserScreen*>(ud);
    const bool     showUp    = (self->cwd_ != "/");
    const IconDef* folderIco = findIcon("file.folder");
    const IconDef* fileIco   = findIcon("file.file");

    ListEntry e;
    if (showUp && idx == 0) {
        e.label = "..";
        if (folderIco) { e.leftIcon = folderIco->bitmap; e.iconW = folderIco->w; e.iconH = folderIco->h; }
    } else {
        int ei = idx - (showUp ? 1 : 0);
        if (ei < 0 || ei >= (int)self->entries_.size()) return nullptr;
        const FsEntry& entry = self->entries_[(size_t)ei];
        e.label   = entry.name.c_str();
        e.chevron = entry.isDir;
        if (!entry.isDir && (size_t)ei < self->accStrs_.size() && !self->accStrs_[ei].empty())
            e.value = self->accStrs_[ei].c_str();
        const IconDef* ico = entry.isDir ? folderIco : fileIco;
        if (ico) { e.leftIcon = ico->bitmap; e.iconW = ico->w; e.iconH = ico->h; }
    }

    UiNode* row = ListItemRow(a, e);
    if (row) row->selfHighlight = focused;
    return row;
}

// ── build ─────────────────────────────────────────────────────────────────────

aether::ui::UiNode* FileBrowserScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    const bool showUp = (cwd_ != "/");
    const int  total  = (int)entries_.size() + (showUp ? 1 : 0);

    UiNode* list;
    if (total == 0) {
        ListEntry e; e.label = "(empty)";
        list = ListItemRow(a, e);
    } else {
        list = VirtualList(a, vlist_, total, kItemH, renderItem, this);
    }

    return View(a, root, {
        ListSection(a, pathBuf_),
        list,
    });
}

} // namespace nema
