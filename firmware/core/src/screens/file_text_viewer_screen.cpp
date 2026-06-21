#include "nema/screens/file_text_viewer_screen.h"
#include "nema/runtime.h"
#include "nema/ui/widgets.h"
#include "nema/hal/filesystem.h"
#include <cstdio>
#include <cstring>

namespace nema {

using namespace aether::ui;

TextViewerScreen::TextViewerScreen(Runtime& rt) : ComponentScreen(rt, 512) {}

void TextViewerScreen::setPath(const char* path) {
    path_ = path ? path : "";
    // Derive display title from basename.
    const char* slash = std::strrchr(path_.c_str(), '/');
    std::snprintf(titleBuf_, sizeof(titleBuf_), "%s", slash ? slash + 1 : path_.c_str());
}

void TextViewerScreen::onResume() {
    scroll_.scrollMain = 0;
    lines_.clear();
    truncated_ = false;

    IFileSystem* fs = rt_.fs();
    if (!fs || path_.empty()) {
        lines_.push_back("(no file)");
        ComponentScreen::onResume();
        return;
    }

    std::vector<uint8_t> data;
    if (!fs->read(path_, data)) {
        lines_.push_back("(read error)");
        ComponentScreen::onResume();
        return;
    }

    if (data.size() > kMaxBytes) {
        data.resize(kMaxBytes);
        truncated_ = true;
    }

    // Split into lines on '\n'.
    const char* p   = reinterpret_cast<const char*>(data.data());
    const char* end = p + data.size();
    const char* lineStart = p;
    while (p <= end && lines_.size() < kMaxLines) {
        if (p == end || *p == '\n') {
            lines_.emplace_back(lineStart, p - lineStart);
            lineStart = p + 1;
        }
        p++;
    }
    if (lines_.size() >= kMaxLines) truncated_ = true;

    ComponentScreen::onResume();
}

aether::ui::UiNode* TextViewerScreen::build(NodeArena& a, Runtime&) {
    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;
    Style sv;   sv.dir = FlexDir::Col;   sv.align = Align::Stretch;

    UiNode* scroll = ScrollView(a, scroll_, sv, {});
    UiNode* prev   = nullptr;

    auto append = [&](UiNode* n) {
        if (!n) return;
        if (!prev) scroll->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };

    for (auto& line : lines_) {
        append(Text(a, line.c_str(), TextRole::Mono));
    }

    if (truncated_) append(Text(a, "... (truncated)", TextRole::Caption));
    if (lines_.empty()) append(Text(a, "(empty)", TextRole::Caption));

    return View(a, root, {
        ListSection(a, titleBuf_),
        scroll,
    });
}

} // namespace nema
