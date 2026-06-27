#pragma once
#include "nema/ui/component_screen.h"
#include "nema/ui/node.h"
#include <string>
#include <vector>

namespace nema {

class Runtime;

// Raw text viewer — reads a file from the VFS and displays its content line
// by line in monospace font. Intended as the default handler for opening any
// file from the file browser. Binary files will display as garbled text, which
// is expected for a "raw" viewer.
class TextViewerScreen : public ComponentScreen {
public:
    explicit TextViewerScreen(Runtime& rt);

    void setPath(const char* path);

    void onResume()      override;
    bool onBackPressed() override { return false; }

    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    void finishRead();   // UI-thread: split readData_ into lines_ after the async read

    static constexpr size_t kMaxBytes = 16 * 1024;
    static constexpr size_t kMaxLines = 480;

    std::string              path_;
    std::vector<std::string> lines_;
    std::vector<uint8_t>     readData_;       // async read buffer (worker → finishRead)
    bool                     readOk_    = false;
    bool                     truncated_ = false;
    aether::ui::ScrollState  scroll_;
    char                     titleBuf_[64] = {};
};

} // namespace nema
