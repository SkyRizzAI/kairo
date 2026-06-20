#include "nema/ui/style_tokens.h"

namespace aether {

static const StyleTokens* s_theme = nullptr;

// ── Built-in themes ───────────────────────────────────────────────────────────

const StyleTokens& defaultTheme() {
    static const StyleTokens t{
        "default",
        /*space*/   {2, 4, 8, 12, 16},
        /*radius*/  {0, 2, 4, 255},
        /*icon*/    {8, 12, 16, 24, 32},
        /*font*/    {1, 1, 1, 1},   // all 1x — roles map to real-size proportional fonts
        /*invertedStatusBar*/ true,
    };
    return t;
}

const StyleTokens& compactTheme() {
    static const StyleTokens t{
        "compact",
        /*space*/   {1, 2, 4, 6, 8},
        /*radius*/  {0, 1, 2, 255},
        /*icon*/    {6, 8, 12, 16, 24},
        /*font*/    {1, 1, 1, 1},   // caption=1x, body=1x, title=1x, subhead=1x — everything tiny
        /*invertedStatusBar*/ true,
    };
    return t;
}

const StyleTokens& largeTheme() {
    static const StyleTokens t{
        "large",
        /*space*/   {4, 8, 12, 16, 24},
        /*radius*/  {0, 4, 8, 255},
        /*icon*/    {12, 16, 24, 32, 48},
        /*font*/    {1, 2, 3, 2},   // caption=1x, body=2x, title=3x, subhead=2x (scales with items)
        /*invertedStatusBar*/ true,
    };
    return t;
}

// ── Active theme ──────────────────────────────────────────────────────────────

const StyleTokens& theme() {
    return s_theme ? *s_theme : defaultTheme();
}

void setTheme(const StyleTokens& t) {
    s_theme = &t;
}

} // namespace aether
