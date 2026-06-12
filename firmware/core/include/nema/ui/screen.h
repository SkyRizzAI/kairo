#pragma once
#include "nema/ui/key.h"
#include "nema/input/input_action.h"
#include "nema/input/pointer.h"
#include <cstdint>

namespace nema {

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

    // Touch/pointer handler (Plan 29). Default no-op; component-based screens
    // (Plan 30) and AppHost override it. Coordinates are logical canvas px.
    virtual void onPointer(const input::PointerEvent& /*e*/) {}

    // Legacy handler — kept for backward compat. Screens that only override
    // this continue to work via the onAction() default forward above.
    virtual void update(Key /*key*/) {}

    virtual void draw(Canvas& canvas) = 0;
    virtual void tick(uint64_t /*nowMs*/) {}

    virtual ScreenMode mode() const { return ScreenMode::Normal; }

    // For Modal mode: size of the floating box (centered by runtime)
    virtual uint16_t modalWidth()  const { return 210; }
    virtual uint16_t modalHeight() const { return 64; }

    // Fullscreen screens that write directly to the display via blitRgb565
    // can return true to prevent GuiService from flushing the 1-bit canvas
    // (which would overwrite their color output). The screen is then solely
    // responsible for what appears on the LCD.
    virtual bool suppressCanvasFlush() const { return false; }
};

} // namespace nema
