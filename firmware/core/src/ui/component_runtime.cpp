#include "kairo/ui/component_runtime.h"
#include "kairo/ui/renderer.h"
#include "kairo/ui/hit_test.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/key.h"
#include <cstdlib>   // abs

namespace kairo::ui {

// Gesture tuning.
static constexpr int   DRAG_THRESH = 6;     // px of movement before a tap → drag
static constexpr float FRICTION    = 0.85f; // momentum decay per tick
static constexpr float MIN_VEL     = 1.0f;  // stop momentum below this

static bool contains(const UiNode* n, int16_t x, int16_t y) {
    return x >= n->x && x < n->x + n->w && y >= n->y && y < n->y + n->h;
}

// Deepest Scroll node whose box contains the point (so nested scrolls work).
static UiNode* scrollAt(UiNode* n, int16_t x, int16_t y) {
    UiNode* found = nullptr;
    if (n->type == NodeType::Scroll && contains(n, x, y)) found = n;
    for (UiNode* k = n->firstChild; k; k = k->nextSibling)
        if (UiNode* c = scrollAt(k, x, y)) found = c;
    return found;
}

// Deepest Slider node containing the point (touch value control).
static UiNode* sliderAt(UiNode* n, int16_t x, int16_t y) {
    UiNode* found = nullptr;
    if (n->type == NodeType::Slider && contains(n, x, y)) found = n;
    for (UiNode* k = n->firstChild; k; k = k->nextSibling)
        if (UiNode* c = sliderAt(k, x, y)) found = c;
    return found;
}

// Map an x within [dsX, dsX+dsW] to a slider value and push it via onChange.
static void sliderSetFromX(ComponentState& st, int16_t x) {
    if (!st.dragSliderValue || st.dsW <= 0) return;
    int range = st.dsMax - st.dsMin;
    int v = st.dsMin + (int)((long)(x - st.dsX) * range / st.dsW);
    if (v < st.dsMin) v = st.dsMin;
    if (v > st.dsMax) v = st.dsMax;
    *st.dragSliderValue = v;
    if (st.dsOnChange) st.dsOnChange(st.dsUserdata, v);
}

static bool isDescendant(const UiNode* anc, const UiNode* target) {
    for (const UiNode* k = anc->firstChild; k; k = k->nextSibling) {
        if (k == target || isDescendant(k, target)) return true;
    }
    return false;
}

// Find the scroll node that owns `target` and nudge scrollMain so target is
// fully inside the viewport. Vertical scroll only. Returns true if changed.
static UiNode* findScrollAncestor(UiNode* n, const UiNode* target) {
    if (n->type == NodeType::Scroll && isDescendant(n, target)) {
        // descend in case of nested scrolls — innermost wins
        for (UiNode* k = n->firstChild; k; k = k->nextSibling)
            if (UiNode* inner = findScrollAncestor(k, target)) return inner;
        return n;
    }
    for (UiNode* k = n->firstChild; k; k = k->nextSibling)
        if (UiNode* s = findScrollAncestor(k, target)) return s;
    return nullptr;
}

static bool ensureVisible(UiNode* root, const UiNode* foc) {
    UiNode* sn = findScrollAncestor(root, foc);
    if (!sn || !sn->scroll || sn->style.dir != FlexDir::Col) return false;
    const int pad = sn->style.padding;
    int top    = sn->y + pad;
    int bottom = sn->y + sn->h - pad;
    int before = sn->scroll->scrollMain;
    int sc = before;
    if (foc->y < top)              sc -= (top - foc->y);
    else if (foc->y + foc->h > bottom) sc += (foc->y + foc->h - bottom);
    if (sc < 0) sc = 0;
    int maxS = sn->scroll->maxScroll();
    if (sc > maxS) sc = maxS;
    sn->scroll->scrollMain = (int16_t)sc;
    return sc != before;
}

void renderComponentFrame(UiNode* root, Canvas& c, ComponentState& st,
                          const TextMetrics& tm, int16_t ox, int16_t oy,
                          uint16_t w, uint16_t h) {
    if (!root) return;
    layout(*root, w, h, tm, ox, oy);
    UiNode* foc = focusedNode(*root, st.focus);
    if (st.modality == input::InputModality::Button && foc) {
        if (ensureVisible(root, foc)) {
            layout(*root, w, h, tm, ox, oy);   // re-flow once with new scroll
            foc = focusedNode(*root, st.focus);
        }
    }
    render(*root, c, st.modality == input::InputModality::Button ? foc : nullptr);
}

bool dispatchPointer(UiNode* root, ComponentState& st, const input::PointerEvent& e) {
    if (!root) return false;
    st.modality = input::InputModality::Pointer;
    const int16_t x = (int16_t)e.x, y = (int16_t)e.y;

    switch (e.phase) {
    case input::PointerPhase::Down: {
        st.downX = st.lastX = x;
        st.downY = st.lastY = y;
        st.dragging = false;
        st.dragSliderValue = nullptr;
        // A slider under the finger takes priority: capture its geometry (so the
        // drag survives tree rebuilds) and set the value immediately.
        if (UiNode* sl = sliderAt(root, x, y)) {
            if (sl->sliderValue) {
                st.dragSliderValue = sl->sliderValue;
                st.dsX = sl->x; st.dsW = (int16_t)sl->w;
                st.dsMin = sl->sliderMin; st.dsMax = sl->sliderMax;
                st.dsOnChange = sl->onChange; st.dsUserdata = sl->userdata;
                st.dragging = true; st.pressed = nullptr; st.dragScroll = nullptr;
                sliderSetFromX(st, x);
                return true;   // geometry captured → safe to rebuild
            }
        }
        st.pressed  = hitTest(*root, x, y);
        UiNode* sn  = scrollAt(root, x, y);
        st.dragScroll = sn ? sn->scroll : nullptr;
        if (st.dragScroll) st.dragScroll->velocity = 0;  // catch a gliding view
        // Deliberately NO redraw on Down: rebuilding the tree here would
        // invalidate st.pressed (arena memory) and break the tap. The focus
        // ring hides on the next natural redraw (the Up/onPress or a drag Move).
        return false;
    }
    case input::PointerPhase::Move: {
        if (st.dragSliderValue) { sliderSetFromX(st, x); st.lastX = x; st.lastY = y; return true; }
        int dy = y - st.lastY;
        st.lastX = x; st.lastY = y;
        if (!st.dragging &&
            (std::abs(x - st.downX) > DRAG_THRESH || std::abs(y - st.downY) > DRAG_THRESH)) {
            st.dragging = true;
            st.pressed  = nullptr;   // movement cancels the tap
        }
        if (st.dragging && st.dragScroll) {
            st.dragScroll->scrollMain = (int16_t)(st.dragScroll->scrollMain - dy);
            st.dragScroll->velocity   = (float)(-dy);   // for release momentum
            return true;
        }
        return false;
    }
    case input::PointerPhase::Up: {
        if (st.dragSliderValue) { st.dragSliderValue = nullptr; st.dragging = false; return false; }
        bool redraw = false;
        if (!st.dragging && st.pressed) {
            UiNode* up = hitTest(*root, x, y);
            if (up && up == st.pressed) {
                if (up->onPress) {
                    up->onPress(up->userdata);
                    redraw = true;
                } else if (up->onAdjust) {
                    // Tap an adjustable (Select/Stepper): left half = prev, right
                    // half = next — matches the ◀ value ▶ affordance.
                    int dir = (x < up->x + up->w / 2) ? -1 : +1;
                    up->onAdjust(up->userdata, dir);
                    redraw = true;
                }
            }
        }
        st.pressed  = nullptr;
        st.dragging = false;
        // dragScroll + its velocity are kept so tickMomentum() can glide.
        return redraw;
    }
    }
    return false;
}

// First Scroll node in tree order (for keyboard scroll fallback on text screens).
static UiNode* firstScroll(UiNode* n) {
    if (n->type == NodeType::Scroll) return n;
    for (UiNode* k = n->firstChild; k; k = k->nextSibling)
        if (UiNode* s = firstScroll(k)) return s;
    return nullptr;
}

static constexpr int SCROLL_STEP = 24;   // px per Prev/Next on a text screen

bool dispatchNav(UiNode* root, ComponentState& st, Nav nav) {
    if (!root) return false;
    st.modality = input::InputModality::Button;   // ring returns
    // Activate: fire the focused control's onPress; if it's an adjustable
    // (Select/Stepper/Slider with no onPress), Activate advances it (+1) — the
    // fallback for boards without Left/Right keys.
    if (nav == Nav::Activate) {
        if (handleFocusKey(*root, st.focus, Key::Select)) return true;
        return dispatchAdjust(root, st, +1);
    }
    Key k = nav == Nav::Prev ? Key::Up : Key::Down;
    if (handleFocusKey(*root, st.focus, k)) return true;
    // No focusable target moved (e.g. a read-only scrolling text screen) — fall
    // back to nudging the scroll view so Prev/Next still pans the content.
    if (nav != Nav::Activate) {
        if (UiNode* sn = firstScroll(root)) {
            if (sn->scroll) {
                sn->scroll->scrollMain =
                    (int16_t)(sn->scroll->scrollMain + (nav == Nav::Next ? SCROLL_STEP : -SCROLL_STEP));
                return true;
            }
        }
    }
    return false;
}

bool dispatchAdjust(UiNode* root, ComponentState& st, int dir) {
    if (!root) return false;
    st.modality = input::InputModality::Button;
    UiNode* foc = focusedNode(*root, st.focus);
    if (!foc) return false;
    if (foc->type == NodeType::Slider && foc->sliderValue) {
        int v = *foc->sliderValue + dir * foc->sliderStep;
        if (v < foc->sliderMin) v = foc->sliderMin;
        if (v > foc->sliderMax) v = foc->sliderMax;
        *foc->sliderValue = v;
        if (foc->onChange) foc->onChange(foc->userdata, v);
        return true;
    }
    if (foc->onAdjust) { foc->onAdjust(foc->userdata, dir); return true; }
    return false;
}

bool tickMomentum(ComponentState& st) {
    if (!st.dragScroll) return false;
    float v = st.dragScroll->velocity;
    if (v > -MIN_VEL && v < MIN_VEL) { st.dragScroll->velocity = 0; return false; }
    st.dragScroll->scrollMain = (int16_t)(st.dragScroll->scrollMain + (int)v);
    st.dragScroll->velocity   = v * FRICTION;
    // If we've run into a clamp bound, the next layout pins scrollMain; kill the
    // glide when it would push past the recorded extents.
    int16_t s = st.dragScroll->scrollMain;
    if (s <= 0 || s >= st.dragScroll->maxScroll()) st.dragScroll->velocity = 0;
    return true;
}

} // namespace kairo::ui
