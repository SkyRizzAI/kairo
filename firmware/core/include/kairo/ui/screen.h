#pragma once
#include "kairo/ui/key.h"
#include "kairo/input/input_action.h"
#include <cstdint>

namespace kairo {

class Canvas;

// How the runtime composites this screen.
enum class ScreenMode : uint8_t {
    Normal,      // status bar auto-drawn before draw(); screen fills CONTENT area
    Fullscreen,  // full canvas, no automatic status bar
    Modal,       // runtime renders previous screen + white backdrop box, then draw()
};

struct IScreen {
    virtual ~IScreen() = default;
    virtual void enter() {}

    // Primary handler — called with the resolved navigation intent.
    // Default forwards to legacy update(Key) for backward compat.
    virtual void onAction(input::Action a) { update(input::keyFromAction(a)); }

    // Raw code handler — for physical-identity-sensitive cases (games, etc).
    virtual void onCode(input::Code /*c*/) {}

    // Legacy handler — kept for backward compat. Screens that only override
    // this continue to work via the onAction() default forward above.
    virtual void update(Key /*key*/) {}

    virtual void draw(Canvas& canvas) = 0;
    virtual void tick(uint64_t /*nowMs*/) {}

    virtual ScreenMode mode() const { return ScreenMode::Normal; }

    // For Modal mode: size of the floating box (centered by runtime)
    virtual uint16_t modalWidth()  const { return 210; }
    virtual uint16_t modalHeight() const { return 64; }
};

} // namespace kairo
