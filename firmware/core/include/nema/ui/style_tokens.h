#pragma once
#include <cstdint>

// Plan 53 — StyleTokens: design tokens for the Aether UI system.
//
// Widgets and screens reference tokens by slot instead of hard-coded pixel
// values. A theme swap (board profile, user preference) changes the whole UI
// at once without touching any screen code.
//
// Token lifecycle: set once by the platform (or config "display/theme") before
// GuiService starts, then read-only for the session. Runtime theme switching
// (Plan 53 Phase 2) will add a change-notification callback.

namespace nema {

// Spacing scale — logical pixels for padding, gaps, and margins.
struct SpaceTokens {
    uint8_t xs;  // 2  — icon inner pad, tiny inline gap
    uint8_t sm;  // 4  — row padding, small gap between items
    uint8_t md;  // 8  — section gap, card inner padding
    uint8_t lg;  // 12 — screen-edge margin
    uint8_t xl;  // 16 — large section gap, hero spacing
};

// Border radius in pixels. Always 0 on 1-bit displays; meaningful for colour.
struct RadiusTokens {
    uint8_t none;  // 0
    uint8_t sm;    // 2
    uint8_t md;    // 4
    uint8_t full;  // 255  (pill shape)
};

// Icon slot sizes in pixels.
struct IconTokens {
    uint8_t xs;  //  8 — status-bar icons
    uint8_t sm;  // 12 — small inline icons
    uint8_t md;  // 16 — list-row icons
    uint8_t lg;  // 24 — launcher grid icons
    uint8_t xl;  // 32 — hero icon
};

// Font render scale (pixel-multiplier applied to the base 5x8 bitmap font).
// scale=1 -> 5x8 px/char, scale=2 -> 10x16 px/char, scale=3 -> 15x24 px/char.
struct FontTokens {
    uint8_t caption;  // 1 — small labels, hints
    uint8_t body;     // 1 — list rows, body text
    uint8_t title;    // 2 — screen headings (page title bars)
    uint8_t subhead;  // 1 — list section headers (bold, smaller than title)
};

// The complete token set for one theme.
struct StyleTokens {
    const char*  name;               // "default" | "compact" | "large"
    SpaceTokens  space;
    RadiusTokens radius;
    IconTokens   icon;
    FontTokens   font;
    bool         invertedStatusBar;  // filled dark band vs. outlined bar
};

// ── Active theme ──────────────────────────────────────────────────────────────
// Call setTheme() before GuiService::start(); read theme() afterwards.
const StyleTokens& theme();
void               setTheme(const StyleTokens& t);

// ── Built-in themes ───────────────────────────────────────────────────────────
const StyleTokens& defaultTheme();   // standard Aether look
const StyleTokens& compactTheme();   // tighter spacing for small displays
const StyleTokens& largeTheme();     // larger targets for accessibility

} // namespace nema
