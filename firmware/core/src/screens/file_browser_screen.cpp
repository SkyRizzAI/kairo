#include "nema/screens/file_browser_screen.h"
#include "nema/runtime.h"
#include "nema/ui/widgets.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/icon_pack.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace nema {

using namespace ui;

// Bigger arena than the default: a directory builds ~4 nodes per row
// (Pressable + Icon + SmartLabel + accessory) plus the chrome.
FileBrowserScreen::FileBrowserScreen(Runtime& rt) : ComponentScreen(rt, 768) {}

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

// ── directory model ─────────────────────────────────────────────────────────

void FileBrowserScreen::reload() {
    entries_.clear();
    if (IFileSystem* fs = rt_.fs()) fs->list(cwd_, entries_);

    // Folders first, then case-insensitive alphabetical (Flipper ordering).
    std::sort(entries_.begin(), entries_.end(), [](const FsEntry& a, const FsEntry& b) {
        if (a.isDir != b.isDir) return a.isDir;
        return ciLess(a.name, b.name);
    });
    if (entries_.size() > kMaxEntries) entries_.resize(kMaxEntries);

    std::snprintf(pathBuf_, sizeof(pathBuf_), "%s", cwd_.c_str());

    // Fresh view each time we enter a directory.
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
    if (entryIndex < 0) { goUp(); return; }   // ".." row
    if (entryIndex >= (int)entries_.size()) return;
    const FsEntry& e = entries_[(size_t)entryIndex];
    if (!e.isDir) return;                      // files: no action (v1)
    cwd_ = (cwd_ == "/") ? ("/" + e.name) : (cwd_ + "/" + e.name);
    reload();
    dirty_ = true;
    requestRedraw();
}

void FileBrowserScreen::onRowPress(void* u) {
    auto* r = static_cast<Row*>(u);
    r->self->openEntry(r->entryIndex);
}

// ── lifecycle ─────────────────────────────────────────────────────────────────

void FileBrowserScreen::onResume() {
    reload();
    ComponentScreen::onResume();
}

bool FileBrowserScreen::onBackPressed() {
    if (cwd_ != "/") { goUp(); return true; }  // consume: navigate up
    return false;                              // at root → dispatcher pops to Home
}

// ── view ─────────────────────────────────────────────────────────────────────

ui::UiNode* FileBrowserScreen::build(NodeArena& a, Runtime&) {
    uint8_t pad = nema::theme().space.sm;
    uint8_t gap = nema::theme().space.xs;

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = pad; root.gap = gap;
    root.align = Align::Stretch;
    Style sv;   sv.dir = FlexDir::Col; sv.align = Align::Stretch; sv.gap = 1;
    Style rs;   rs.dir = FlexDir::Row; rs.padding = pad; rs.align = Align::Center; rs.gap = gap;
    rs.justify = Justify::SpaceBetween;

    const bool   showUp = (cwd_ != "/");
    const size_t total  = entries_.size() + (showUp ? 1 : 0);
    rows_.resize(total);
    accStrs_.resize(total);

    const IconDef* folderIco = findIcon("file.folder");
    const IconDef* fileIco   = findIcon("file.file");

    UiNode* list = ScrollView(a, scroll_, sv, {});
    UiNode* prev = nullptr;
    size_t  ri   = 0;

    // Link a row: [icon] [name (grows)] [accessory]. Returns false if arena full.
    auto addRow = [&](const IconDef* ico, const char* name,
                      std::string acc, int entryIndex) -> bool {
        rows_[ri]    = { this, entryIndex };
        accStrs_[ri] = std::move(acc);

        UiNode* icoNode = ico ? Icon(a, ico->bitmap, ico->w, ico->h) : nullptr;
        UiNode* lbl     = SmartLabel(a, name);
        if (lbl) lbl->style.flexGrow = 1;
        UiNode* accNode = accStrs_[ri].empty()
                              ? nullptr
                              : Text(a, accStrs_[ri].c_str(), TextRole::Caption);

        UiNode* row = a.alloc();
        if (!row) return false;
        row->type      = NodeType::Pressable;
        row->style     = rs;
        row->onPress   = onRowPress;
        row->userdata  = &rows_[ri];
        row->focusable = true;

        UiNode* cprev = nullptr;
        auto link = [&](UiNode* ch) {
            if (!ch) return;
            if (!cprev) row->firstChild = ch; else cprev->nextSibling = ch;
            cprev = ch;
        };
        link(icoNode);
        link(lbl);
        link(accNode);

        if (!prev) list->firstChild = row; else prev->nextSibling = row;
        prev = row;
        ri++;
        return true;
    };

    if (showUp) addRow(folderIco, "..", std::string(), -1);

    char sizeBuf[16];
    for (size_t i = 0; i < entries_.size(); i++) {
        const FsEntry& e = entries_[i];
        std::string acc;
        if (e.isDir) {
            acc = ">";
        } else {
            formatSize(sizeBuf, sizeof(sizeBuf), e.size);
            acc = sizeBuf;
        }
        if (!addRow(e.isDir ? folderIco : fileIco, e.name.c_str(), std::move(acc), (int)i))
            break;   // arena exhausted — stop gracefully
    }

    if (total == 0)
        list->firstChild = SmartLabel(a, "(empty)");

    return View(a, root, {
        TitleBar(a, "FILES"),
        Text(a, pathBuf_, TextRole::Caption),
        list,
    });
}

} // namespace nema
