#pragma once
#include "kairo/ui/node.h"
#include "kairo/ui/key.h"

namespace kairo::ui {

struct FocusState {
    int focused = 0;   // index into focusable nodes (tree order)
    int count   = 0;   // updated each handleFocusKey / focusedNode call
};

// The currently-focused Pressable node, or nullptr. Also refreshes fs.count and
// clamps fs.focused into range. Call before render() to get the highlight target.
UiNode* focusedNode(UiNode& root, FocusState& fs);

// Move focus (Up/Left = prev, Down/Right = next, wrap) or activate (Select fires
// the focused node's onPress). Returns true if something changed / fired and a
// redraw is warranted. Cancel is NOT handled here (bubbles to the app).
bool handleFocusKey(UiNode& root, FocusState& fs, Key k);

} // namespace kairo::ui
