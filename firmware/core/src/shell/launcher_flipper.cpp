// Flipper-Zero-style list launcher skin ("flipper").
//
// Plain vertical list: each row is  [icon]  Label
// The focused row is highlighted with a filled rounded selection box; the
// icon and label are drawn inverted (black on white).  All other rows are
// white on the dark background.
//
// Animated icons work for free: LauncherScreen::draw() updates e.icon to the
// current animation frame before calling theme_->draw(), so this skin just
// renders e.icon without knowing whether it animates.
#include "aether/shell/launcher_flipper.h"
#include "aether/shell/blit.h"
#include "nema/ui/canvas.h"
#include "nema/ui/draw.h"
#include "nema/ui/text_style.h"
#include "nema/ui/ui_constants.h"

namespace nema::shell {

using namespace aether::ui;

void FlipperLauncher::draw(nema::Canvas& c, const LauncherModel& m, int cursor) {
    const uint16_t W = c.width(), H = c.height();
    const int n = m.count;
    if (n <= 0) return;
    if (cursor < 0) cursor = 0; else if (cursor >= n) cursor = n - 1;

    const uint16_t top  = nema::display::contentY();
    const uint16_t sbW  = 3;                              // vertical scrollbar width
    const uint16_t area = (uint16_t)(H > top ? H - top : 0);
    if (area < 4) return;

    // Row height derived from body font so it scales with UI theme.
    const uint16_t textH = measureTextH(TextRole::Body);
    const uint16_t rowH  = (uint16_t)(textH + 4);         // 2px top + 2px bottom
    const uint16_t iconSz = (uint16_t)(rowH > 4 ? rowH - 4 : 1);
    const uint16_t rowW  = (uint16_t)(W > sbW + 1 ? W - sbW - 1 : W); // leave room for scrollbar
    const uint16_t padX  = 3;                             // left edge → icon
    const uint16_t gap   = 3;                             // icon right → label left

    const int visRows = (area / rowH > 0) ? (int)(area / rowH) : 1;

    // Keep cursor centred in the visible window.
    int scroll = cursor - visRows / 2;
    if (scroll > n - visRows) scroll = n - visRows;
    if (scroll < 0)           scroll = 0;

    FontSpec fs = fontForRole(TextRole::Body);
    c.setFont(fs.handle);

    for (int i = scroll; i < scroll + visRows && i < n; i++) {
        const bool sel = (i == cursor);
        const uint16_t rowY = (uint16_t)(top + (i - scroll) * rowH);

        if (sel) {
            draw::box_rounded(c, 0, rowY, rowW, rowH, true);
        }

        const bool fg = !sel;   // white on dark / black on white
        const LauncherEntry& e = m.items[i];

        // Icon centred vertically in the row.
        if (e.icon && e.iconW && e.iconH) {
            uint16_t ix = padX;
            uint16_t iy = (uint16_t)(rowY + (rowH - iconSz) / 2);
            blitScaledMask(c, e.icon, e.iconW, e.iconH, ix, iy, iconSz, iconSz, fg);
        }

        // Label.
        if (e.label) {
            uint16_t lx = (uint16_t)(padX + iconSz + gap);
            uint16_t ly = (uint16_t)(rowY + (rowH - textH) / 2);
            if (fs.scale <= 1) c.drawText(lx, ly, e.label, fg);
            else               c.drawTextScaled(lx, ly, e.label, fs.scale, fg);
        }
    }

    // Vertical scrollbar — only when content overflows the visible area,
    // matching the component renderer's behaviour.
    if (n > visRows) {
        draw::scrollbar(c, (uint16_t)(W - sbW), top, area,
                        (uint16_t)scroll, (uint16_t)visRows, (uint16_t)n, false);
    }
}

} // namespace nema::shell
