#pragma once
#include "nema/ui/key.h"
#include "nema/input/input_action.h"
#include "nema/input/pointer.h"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace nema {

class Canvas;

// How the runtime composites this screen.
enum class ScreenMode : uint8_t {
    Normal,      // status bar auto-drawn before draw(); screen fills CONTENT area
    Fullscreen,  // full canvas, no automatic status bar
    Modal,       // runtime renders previous screen + white backdrop box, then draw()
};

// Plan 70: simple key-value store for screen arguments and saved state.
class Bundle {
public:
    void putString(const char* key, const char* value) {
        entries_.push_back({key, Entry::STRING, value, 0, false});
    }
    void putInt(const char* key, int32_t value) {
        entries_.push_back({key, Entry::INT, "", value, false});
    }
    void putBool(const char* key, bool value) {
        entries_.push_back({key, Entry::BOOL, "", 0, value});
    }

    const char* getString(const char* key, const char* defaultVal = "") const {
        for (auto& e : entries_)
            if (e.key == key && e.type == Entry::STRING) return e.strVal.c_str();
        return defaultVal;
    }
    int32_t getInt(const char* key, int32_t defaultVal = 0) const {
        for (auto& e : entries_)
            if (e.key == key && e.type == Entry::INT) return e.intVal;
        return defaultVal;
    }
    bool getBool(const char* key, bool defaultVal = false) const {
        for (auto& e : entries_)
            if (e.key == key && e.type == Entry::BOOL) return e.boolVal;
        return defaultVal;
    }

    bool hasKey(const char* key) const {
        for (auto& e : entries_)
            if (e.key == key) return true;
        return false;
    }
    size_t size() const { return entries_.size(); }

private:
    struct Entry {
        std::string key;
        enum { STRING, INT, BOOL } type;
        std::string strVal;
        int32_t     intVal = 0;
        bool        boolVal = false;
    };
    std::vector<Entry> entries_;
};

struct IScreen {
    virtual ~IScreen() = default;

    // ── Plan 70: Android-style lifecycle ────────────────────────────────
    // onResume: screen is now the active (topmost) screen. Called on push,
    //           on back-navigation reveal, and after pause.
    virtual void onResume() {}

    // onPause: another screen is about to cover this one. Use for saving
    //          transient state, stopping animations, releasing resources.
    virtual void onPause() {}

    // onStop: screen is no longer visible (fully covered or popped).
    virtual void onStop() {}

    // onBackPressed: returns true if the screen consumed the back press
    //                (prevents the dispatcher from popping).
    virtual bool onBackPressed() { return false; }

    // onSaveInstanceState: persist state across stop/destroy cycles.
    virtual void onSaveInstanceState(Bundle& /*out*/) {}

    // ── Deprecated (Plan 70): kept for backward-compat, forwards to onResume ─
    virtual void enter() { onResume(); }

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
