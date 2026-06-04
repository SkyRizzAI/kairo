#pragma once
#include "kairo/ui/key.h"
#include "kairo/input/input_action.h"
#include "kairo/input/pointer.h"
#include <vector>
#include <cstdint>

namespace kairo {

struct IScreen;

class ViewDispatcher {
public:
    void push(IScreen& screen);   // push onto stack + calls screen.enter()
    void pop();                   // pop and call enter() on revealed screen
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
    bool redrawPending_ = false;
};

} // namespace kairo
