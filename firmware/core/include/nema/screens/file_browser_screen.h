#pragma once
// File Browser — Flipper-style VFS file manager (scrolling icon list).
//
// Built entirely on the Aether component system, which natively provides the
// pieces Flipper hand-draws: ScrollView (auto-scrolls the focused row into
// view + dashed scrollbar), inverted selection highlight, and SmartLabel
// marquee for long names when a row is focused. Folders descend on Activate;
// Back goes up one level (and exits to Home at root).
#include "nema/ui/component_screen.h"
#include "nema/ui/node.h"            // aether::ui::ScrollState
#include "nema/hal/filesystem.h"     // FsEntry
#include <vector>
#include <string>

namespace nema {

class Runtime;

class FileBrowserScreen : public ComponentScreen {
public:
    explicit FileBrowserScreen(Runtime& rt);

    void        onResume()      override;   // (re)list the current directory
    bool        onBackPressed() override;   // Back = up a dir, else exit to Home
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    // Per-row callback payload. entryIndex == -1 is the synthetic ".." row.
    struct Row { FileBrowserScreen* self; int entryIndex; };

    static constexpr size_t kMaxEntries = 128;  // cap so the arena can't overflow

    void reload();              // re-list cwd_ (sorted: dirs first, then alpha)
    void openEntry(int entryIndex);
    void goUp();

    static void onRowPress(void* u);

    std::string              cwd_ = "/";
    std::vector<FsEntry>     entries_;
    std::vector<Row>         rows_;      // stable userdata for Pressable rows
    std::vector<std::string> accStrs_;   // stable accessory text (sizes / ">")
    aether::ui::ScrollState          scroll_;
    char                     pathBuf_[128] = "/";
};

} // namespace nema
