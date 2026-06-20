// Plan 60 Fase 3 — components rewrite with tier-1 draw toolkit.
#include "nema/ui/components.h"
#include "nema/ui/canvas.h"
#include "nema/ui/draw.h"
#include "nema/ui/ui_constants.h"
#include "nema/ui/style_tokens.h"
#include "nema/ui/text_style.h"

namespace nema {
} // namespace nema

namespace aether::ui {
using namespace nema;  // Plan 80: nema core symbols (Canvas/Key/input/anim/fonts) in scope

uint16_t drawTitle(Canvas& c, const char* title) {
    const aether::StyleTokens& t = aether::theme();
    uint16_t bannerH = (uint16_t)(measureTextH(TextRole::Title) + 2 * t.space.xs);
    uint16_t bannerY = CONTENT_Y;
    aether::ui::draw::banner(c, 0, bannerY, c.width(), bannerH, title, false);
    return (uint16_t)(bannerY + bannerH + t.space.xs);
}

uint16_t modalOriginX(const Canvas& c, uint16_t w) {
    return (c.width()  > w) ? (uint16_t)((c.width()  - w) / 2) : 0;
}
uint16_t modalOriginY(const Canvas& c, uint16_t h) {
    return (c.height() > h) ? (uint16_t)((c.height() - h) / 2) : 0;
}

void drawModalBox(Canvas& c, uint16_t w, uint16_t h) {
    uint16_t mx = modalOriginX(c, w);
    uint16_t my = modalOriginY(c, h);
    c.fillRect(mx, my, w, h, false);
    aether::ui::draw::box_rounded(c, mx, my, w, h, false);
}

void drawConfirm(Canvas& c, const char* prompt, int cursor, uint16_t w, uint16_t h) {
    uint16_t mx = modalOriginX(c, w);
    uint16_t my = modalOriginY(c, h);

    c.fillRect(mx, my, w, h, false);
    aether::ui::draw::box_rounded(c, mx, my, w, h, false);

    const aether::StyleTokens& t = aether::theme();
    uint16_t pad = t.space.sm;
    uint16_t bh  = (uint16_t)(measureTextH(TextRole::Body) + 2 * pad);

    if (prompt)
        aether::ui::draw::ellipsis(c, (uint16_t)(mx + pad), (uint16_t)(my + pad),
                                   (uint16_t)(w - 2 * pad), prompt, TextRole::Body);

    uint16_t sepY = (uint16_t)(my + h - bh - 2);
    aether::ui::draw::separator(c, (uint16_t)(mx + 1), sepY, (uint16_t)(w - 2), true);

    uint16_t bw    = (uint16_t)((w - 3 * pad) / 2);
    uint16_t byesX = (uint16_t)(mx + pad);
    uint16_t bnoX  = (uint16_t)(mx + w - pad - bw);
    uint16_t btnY  = (uint16_t)(sepY + 2);

    auto drawBtn = [&](uint16_t bx, const char* label, bool selected) {
        if (selected) {
            aether::ui::draw::box_rounded(c, bx, btnY, bw, bh, true);
            uint16_t tw = measureTextW(label, TextRole::Body);
            uint16_t tx = (tw < bw) ? (uint16_t)(bx + (bw - tw) / 2) : bx;
            FontSpec fs = fontForRole(TextRole::Body);
            c.setFont(fs.handle);
            c.drawText(tx, (uint16_t)(btnY + pad), label, false);
        } else {
            aether::ui::draw::box_rounded(c, bx, btnY, bw, bh, false);
            uint16_t tw = measureTextW(label, TextRole::Body);
            uint16_t tx = (tw < bw) ? (uint16_t)(bx + (bw - tw) / 2) : bx;
            FontSpec fs = fontForRole(TextRole::Body);
            c.setFont(fs.handle);
            c.drawText(tx, (uint16_t)(btnY + pad), label, true);
        }
    };

    drawBtn(byesX, "Yes", cursor == 0);
    drawBtn(bnoX,  "No",  cursor == 1);
}

} // namespace ui

