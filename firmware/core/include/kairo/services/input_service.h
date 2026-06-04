#pragma once
#include "kairo/nema/input_event.h"
#include "kairo/nema/message_queue.h"
#include "kairo/input/input_code.h"
#include "kairo/input/input_action.h"
#include "kairo/ui/key.h"

namespace kairo::input { class IKeyMap; }

namespace kairo {

// InputService — the single funnel for all input into the system.
//
// Input sources call post() from their own thread; the main task drains via
// next() each frame. Extended in Plan 27 with a keymap slot: when a keymap is
// installed (hardware board), events arrive pre-enriched with Code + Action +
// Gesture. Without a keymap (simulator / legacy path), post(Key) auto-fills
// code/action from the default reduction table.
class InputService {
public:
    // ── Post (called from any thread — thread-safe) ───────────────────────

    // Legacy path: auto-fills code/action/gesture from key.
    void post(Key k, InputEvent::Type t = InputEvent::Type::Press);

    // Full-event post: used by IKeyMap after gesture resolution.
    void post(const InputEvent& e) { queue_.send(e); }

    // Pointer/touch post (Plan 29): same funnel, kind = Pointer. Coordinates
    // must be LOGICAL (the ITouchDriver transforms raw → logical). type=Press
    // so the GUI drain (which filters Press/Repeat) lets it through.
    void postPointer(input::PointerPhase ph, uint16_t x, uint16_t y) {
        InputEvent e;
        e.kind   = InputEvent::Kind::Pointer;
        e.type   = InputEvent::Type::Press;
        e.pphase = ph;
        e.px = x; e.py = y;
        queue_.send(e);
    }

    // ── Drain (main task only) ────────────────────────────────────────────
    bool next(InputEvent& out) { return queue_.tryReceive(out); }

    // ── Keymap (set once at board init) ──────────────────────────────────
    void setKeyMap(input::IKeyMap* km);  // does NOT take ownership
    input::IKeyMap* keyMap() const { return keyMap_; }

    // ── Registry / introspection (delegates to keymap if present) ────────
    const char* hintFor(input::Action a) const;
    bool        hasCode(input::Code c)   const;
    bool        canReach(input::Action a) const;

    // Validate that all four floor Actions are reachable — call after setKeyMap.
    // Returns false on failure; caller should log fatal + halt.
    bool validateFloor() const;

private:
    nema::MessageQueue<InputEvent> queue_{32};
    input::IKeyMap*                keyMap_ = nullptr;
};

} // namespace kairo
