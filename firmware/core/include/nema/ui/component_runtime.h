#pragma once
#include "nema/ui/node.h"
#include "nema/ui/focus.h"
#include "nema/ui/layout.h"
#include "nema/input/pointer.h"

namespace nema {
class Canvas;
}

namespace aether::ui {
using namespace nema;  // Plan 80: nema core symbols (Canvas/Key/input/anim/fonts) in scope

// ComponentRuntime — the shared build→layout→focus→render + input loop body,
// extracted from ComponentApp so both apps (ComponentApp) and system screens
// (ComponentScreen) get identical behaviour, including scroll auto-follow,
// touch drag-scroll, tap-vs-drag discrimination and flick momentum.
//
// The caller owns the UiNode tree (rebuilt each frame from a NodeArena) and a
// ComponentState that persists interaction state across frames.

enum class Nav : uint8_t { Prev, Next, Activate };

struct ComponentState {
    FocusState           focus;
    input::InputModality modality = input::InputModality::Button;

    // Tap tracking: node hit on Down; a release over the same node = click.
    // (UiNode* is arena memory — only valid within one no-rebuild Down→Up.)
    UiNode* pressed = nullptr;

    // Drag/scroll gesture state.
    bool         dragging   = false;
    int16_t      downX = 0, downY = 0;   // pointer-down origin (tap-vs-drag)
    int16_t      lastX = 0, lastY = 0;   // previous move sample
    ScrollState* dragScroll = nullptr;   // caller-owned → safe across rebuilds

    // Active slider drag — geometry/callbacks captured on Down so the drag keeps
    // working after the tree is rebuilt (slider UiNode* would go stale).
    int*    dragSliderValue = nullptr;
    int16_t dsX = 0, dsW = 0, dsMin = 0, dsMax = 0;
    void  (*dsOnChange)(void*, int) = nullptr;
    void*   dsUserdata = nullptr;
};

// Layout the tree into [ox,oy,w,h], auto-scroll the focused node into view (in
// Button modality), then render. Focus ring shows only in Button modality.
void renderComponentFrame(UiNode* root, Canvas& c, ComponentState& st,
                          const TextMetrics& tm, int16_t ox, int16_t oy,
                          uint16_t w, uint16_t h);

// Feed a pointer sample. Distinguishes tap (fires onPress on release) from drag
// (scrolls the ScrollView under the finger; arms momentum on release). Returns
// true if the frame should be redrawn.
bool dispatchPointer(UiNode* root, ComponentState& st, const input::PointerEvent& e);

// Button navigation (Prev/Next move focus, Activate fires onPress). Returns
// true if handled — the caller redraws (renderComponentFrame auto-scrolls).
bool dispatchNav(UiNode* root, ComponentState& st, Nav nav);

// Fine-adjust the focused value control (Slider etc.) by dir (−1/+1). Returns
// true if the focused node consumed it (so the caller can fall back to nav).
bool dispatchAdjust(UiNode* root, ComponentState& st, int dir);

// Advance flick momentum one step. Call from the per-tick path. Returns true
// while the view is still gliding (caller should keep redrawing).
bool tickMomentum(ComponentState& st);

} // namespace aether::ui
