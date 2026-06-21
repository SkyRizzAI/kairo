#pragma once
#include "nema/ui/component_screen.h"
#include "nema/ui/node.h"

namespace nema {

class Runtime;

// FileOpsModal — context menu pushed when the user long-presses (Action::Menu)
// on a file or directory in the file browser. Shows Copy / Cut / Paste /
// Rename / Delete (+ View for files). Delete switches to an inline confirmation
// state before executing. All operations are delegated back to the caller via
// the Callbacks struct; the modal dismisses itself (goBack) after each action.
class FileOpsModal : public ComponentScreen {
public:
    struct Callbacks {
        void (*onView)  (void* u) = nullptr;  // nullptr = hide "View" item
        void (*onCopy)  (void* u) = nullptr;
        void (*onCut)   (void* u) = nullptr;
        void (*onPaste) (void* u) = nullptr;  // nullptr = hide "Paste" item
        void (*onRename)(void* u) = nullptr;
        void (*onDelete)(void* u) = nullptr;
        void* user = nullptr;
    };

    explicit FileOpsModal(Runtime& rt);

    void setup(const char* name, bool isDir, Callbacks cb);

    void        onResume() override;
    aether::ui::UiNode* build(aether::ui::NodeArena& a, Runtime& rt) override;

private:
    enum class St { Menu, ConfirmDelete };

    Callbacks cb_;
    St        st_    = St::Menu;
    bool      isDir_ = false;
    char      name_[64]        = {};
    char      confirmBody_[80] = {};

    aether::ui::ScrollState scroll_;

    static void sView        (void* u);
    static void sCopy        (void* u);
    static void sCut         (void* u);
    static void sPaste       (void* u);
    static void sRename      (void* u);
    static void sDelete      (void* u);
    static void sDeleteConfirm(void* u);
    static void sBack        (void* u);
};

} // namespace nema
