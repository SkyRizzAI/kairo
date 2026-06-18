#pragma once
#include "nema/ui/key.h"
#include "nema/ui/screen.h"
#include "nema/input/input_action.h"
#include "nema/input/pointer.h"
#include <vector>
#include <cstdint>
#include <atomic>

namespace nema {

class ViewDispatcher {
public:
    // ── Plan 70: Android-style navigation API ──────────────────────────

    // navigate: push screen onto stack. Calls onPause() on current, then
    //           push, then onResume() on the new top.
    void navigate(IScreen& screen);

    // replace: swap the top screen without growing the back stack.
    //          Calls onPause() + onStop() on old, remove, navigate new.
    void replace(IScreen& screen);

    // goBack: pop the top screen and reveal the one below. Calls onPause()
    //         + onStop() on current, pop, onResume() on revealed.
    //         Returns false if there is nothing to pop.
    bool goBack();

    // canGoBack: true if there is at least one screen below the top.
    bool canGoBack() const;

    // clearBackStack: remove all screens below the current one.
    void clearBackStack();

    // popTo: pop screens until `target` becomes the top (must be in stack).
    void popTo(IScreen& target);

    // navigate with arguments — passes a Bundle to the next screen's onResume.
    void navigate(IScreen& screen, Bundle args);

    // Retrieve arguments passed to the topmost screen.
    const Bundle& arguments() const { return args_; }

    // ── Legacy API (deprecated, forwards to new API) ────────────────────
    void push(IScreen& screen);   // → navigate(screen)
    void pop();                   // → goBack()
    void popToRoot();             // pop down to the base screen — Plan 22 pause

    IScreen* active()   const;
    IScreen* previous() const;   // second-from-top (for Modal backdrop)
    bool empty() const;

    void requestRedraw();
    // Plan 70: partial redraw — only the given region is dirty.
    void requestRedraw(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    bool takeRedraw();            // consume flag — call in main loop
    // Plan 70: get the dirty bounding box (valid after takeRedraw returns true).
    // Returns false if the dirty rect is the full canvas (no-op optimization).
    bool getDirtyBounds(uint16_t& x, uint16_t& y, uint16_t& w, uint16_t& h) const;

    // Primary dispatch — calls active()->onAction(a).
    void handleAction(input::Action a);

    // Raw code dispatch — calls active()->onCode(c).
    void handleCode(input::Code c);

    // Pointer/touch dispatch — calls active()->onPointer(e).
    void handlePointer(const input::PointerEvent& e);

    // Legacy dispatch — kept for backward compat (simulator stdin uses Key).
    void handleKey(Key key);

    void tick(uint64_t nowMs);

private:
    std::vector<IScreen*> stack_;
    Bundle                args_;        // Plan 70: arguments for topmost screen
    // Dirty region tracking (Plan 70)
    bool     dirtyAll_   = true;        // true = full redraw needed
    uint16_t dirtyX_     = 0, dirtyY_ = 0, dirtyW_ = 0, dirtyH_ = 0;
    bool     hasDirty_   = false;       // false = no dirty rect stored
    // Set by requestRedraw() (often from the app thread via AppHost::present) and
    // consumed by the GUI loop's takeRedraw(). MUST be atomic: with a plain bool
    // the compiler can hoist the GUI loop's read into a register and never observe
    // the app thread's write — on WASM (Web Workers + SharedArrayBuffer) this left
    // a just-loaded app blank until some GUI-side event flipped the flag.
    std::atomic<bool> redrawPending_{false};
};

} // namespace nema
