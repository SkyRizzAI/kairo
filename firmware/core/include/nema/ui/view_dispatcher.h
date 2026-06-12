#pragma once
#include "nema/ui/key.h"
#include "nema/input/input_action.h"
#include "nema/input/pointer.h"
#include <vector>
#include <cstdint>
#include <atomic>

namespace nema {

struct IScreen;

class ViewDispatcher {
public:
    void push(IScreen& screen);   // push onto stack + calls screen.enter()
    void pop();                   // pop and call enter() on revealed screen
    void popToRoot();             // pop down to the base screen (Home) — Plan 22 pause
    IScreen* active()   const;
    IScreen* previous() const;   // second-from-top (for Modal backdrop)
    bool empty() const;

    void requestRedraw();
    bool takeRedraw();            // consume flag — call in main loop

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
    // Set by requestRedraw() (often from the app thread via AppHost::present) and
    // consumed by the GUI loop's takeRedraw(). MUST be atomic: with a plain bool
    // the compiler can hoist the GUI loop's read into a register and never observe
    // the app thread's write — on WASM (Web Workers + SharedArrayBuffer) this left
    // a just-loaded app blank until some GUI-side event flipped the flag.
    std::atomic<bool> redrawPending_{false};
};

} // namespace nema
