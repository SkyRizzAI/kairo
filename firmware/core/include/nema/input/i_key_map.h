#pragma once
#include "nema/input/input_code.h"
#include "nema/input/input_action.h"
#include "nema/input/gesture.h"
#include <cstdint>

namespace nema {
class InputService;
} // namespace nema

namespace nema::input {

// IKeyMap — the single per-board translation layer from physical buttons to
// InputCode + InputAction + Gesture.
//
// A board implements exactly one IKeyMap. It owns a GestureEngine internally.
// When a gesture fires, it calls emitEvent() which posts to InputService.
//
// Responsibilities:
//   - Own the GestureEngine (timing: short/long/repeat).
//   - Map (button_id, Gesture) → (Code, Action).
//   - Provide board-specific hint labels for each Action.
//   - Declare which Codes are available (for capability system).
//   - Pass validateFloor() — all four floor Actions must be reachable.
class IKeyMap {
public:
    virtual ~IKeyMap() = default;

    // Called by InputService; stores the pointer for emitEvent().
    void attachInput(nema::InputService* svc) { input_ = svc; }

    // Called from button poll thread on each press/release edge.
    // Feeds into the internal GestureEngine; emits when a gesture completes.
    virtual void feedEdge(uint8_t buttonId, bool pressed, uint64_t nowMs) = 0;

    // Called every poll tick for long-press / repeat detection.
    virtual void tick(uint64_t nowMs) = 0;

    // ── Introspection (read-only, used by ControlsScreen) ─────────────────

    virtual const char* boardName()   const = 0;   // e.g. "skyrizz-e32"
    virtual int         buttonCount() const = 0;   // physical button count
    virtual const char* buttonLabel(uint8_t id) const = 0;  // "Left"/"Middle"/...

    // Board-specific display label for an Action.
    // Example: Action::Back → "Cancel" (6-btn), "Hold ●" (3-btn), "◀+▶" (2-btn)
    virtual const char* hintFor(Action a) const = 0;

    // Whether this board can produce a given Code natively (not via gesture).
    virtual bool hasCode(Code c) const = 0;

    // Whether this board can reach a given Action (possibly via gesture).
    // Default impl returns true for all floor Actions if validateFloor() passes.
    virtual bool canReach(Action a) const { (void)a; return true; }

    // ── Floor validation ───────────────────────────────────────────────────

    // Checks that Prev, Next, Activate, Back are all reachable.
    // Called at board init; returns false → caller should halt / log fatal.
    bool validateFloor() const;

protected:
    // Subclasses call this when a gesture is ready to emit.
    // Fills in key (backward compat) + code + action + gesture, posts to queue.
    void emitEvent(Code code, Action action, Gesture gesture, uint64_t nowMs);

    nema::InputService* input_ = nullptr;
};

} // namespace nema::input
