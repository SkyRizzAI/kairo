#pragma once
// Plan 81 — Launcher skin interface.
//
// The LauncherScreen owns ALL behaviour (the item model, the cursor, navigation,
// Activate routing). A launcher *theme* owns only the LOOK: given the model and
// the current cursor it paints the menu. Swapping the skin (PlayStation carousel
// ↔ Wii grid) therefore never changes what the launcher does — only how it draws.
#include <cstdint>

namespace nema { class Canvas; }

namespace nema::shell {

// One launcher tile/slot. Behaviour (what Activate does) lives in LauncherScreen,
// keyed by index — the theme only needs what it takes to draw a slot.
struct LauncherEntry {
    const char*    label = nullptr;
    const uint8_t* icon  = nullptr;   // optional 1-bit XBM (row-major, MSB first)
    uint8_t        iconW = 0;
    uint8_t        iconH = 0;
};

// The full set of slots + chrome the skin draws. Caller-owned, rebuilt per frame.
struct LauncherModel {
    const char*          title = nullptr;   // banner title (PS skin); may be null
    const LauncherEntry* items = nullptr;
    int                  count = 0;
};

// A launcher skin. Stateless drawer — no per-instance UI state (the screen holds
// the cursor). columns() is a LAYOUT hint (1 = horizontal carousel row, 2 = grid)
// used only by the skin's own draw(); navigation stays linear in the screen.
struct ILauncherTheme {
    virtual ~ILauncherTheme() = default;
    virtual const char* name()    const = 0;   // "playsta" | "wii"
    virtual int         columns() const = 0;   // visual layout hint
    virtual void        draw(nema::Canvas& c, const LauncherModel& m, int cursor) = 0;
};

} // namespace nema::shell
