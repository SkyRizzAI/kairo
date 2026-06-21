// Plan 81 — shared 1-bit blit helpers.
#include "nema/shell/blit.h"
#include "nema/ui/canvas.h"

namespace nema::shell {

// Anchor → placement fractions (0=near edge, .5=center, 1=far edge) per axis.
static void anchorFrac(Anchor a, float& fx, float& fy) {
    int i = (int)a, col = i % 3, row = i / 3;
    fx = col == 0 ? 0.0f : col == 1 ? 0.5f : 1.0f;
    fy = row == 0 ? 0.0f : row == 1 ? 0.5f : 1.0f;
}

static inline bool srcBit(const uint8_t* bits, int sw, int sx, int sy) {
    uint32_t idx = (uint32_t)sy * sw + sx;
    return (bits[idx / 8] >> (7 - (idx % 8))) & 1;
}

void blitFit(nema::Canvas& c, const uint8_t* bits, int sw, int sh,
             int tx, int ty, int tw, int th, FitMode fit, Anchor anchor) {
    if (!bits || sw <= 0 || sh <= 0 || tw <= 0 || th <= 0) return;

    float scaleX, scaleY;
    switch (fit) {
        case FitMode::Stretch: scaleX = (float)tw / sw; scaleY = (float)th / sh; break;
        case FitMode::Crop: {  // cover: uniform max
            float s = (float)tw / sw, sy = (float)th / sh; if (sy > s) s = sy;
            scaleX = scaleY = s; break;
        }
        case FitMode::Fit: {   // contain: uniform min
            float s = (float)tw / sw, sy = (float)th / sh; if (sy < s) s = sy;
            scaleX = scaleY = s; break;
        }
        case FitMode::Center: default: scaleX = scaleY = 1.0f; break;
    }

    float dwF = sw * scaleX, dhF = sh * scaleY;
    float fx, fy; anchorFrac(anchor, fx, fy);
    float originX = tx + (tw - dwF) * fx;
    float originY = ty + (th - dhF) * fy;

    int x0 = (int)originX;          if (x0 < tx) x0 = tx;
    int y0 = (int)originY;          if (y0 < ty) y0 = ty;
    int x1 = (int)(originX + dwF);  if (x1 > tx + tw) x1 = tx + tw;
    int y1 = (int)(originY + dhF);  if (y1 > ty + th) y1 = ty + th;

    float invX = 1.0f / scaleX, invY = 1.0f / scaleY;
    for (int y = y0; y < y1; y++) {
        int sy = (int)((y - originY) * invY);
        if (sy < 0) sy = 0; else if (sy >= sh) sy = sh - 1;
        for (int x = x0; x < x1; x++) {
            int sx = (int)((x - originX) * invX);
            if (sx < 0) sx = 0; else if (sx >= sw) sx = sw - 1;
            c.drawPixel((uint16_t)x, (uint16_t)y, srcBit(bits, sw, sx, sy));
        }
    }
}

void blitScaledMask(nema::Canvas& c, const uint8_t* bits, int sw, int sh,
                    int dx, int dy, int dw, int dh, bool color) {
    if (!bits || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    for (int y = 0; y < dh; y++) {
        int sy = y * sh / dh; if (sy >= sh) sy = sh - 1;
        for (int x = 0; x < dw; x++) {
            int sx = x * sw / dw; if (sx >= sw) sx = sw - 1;
            if (srcBit(bits, sw, sx, sy))
                c.drawPixel((uint16_t)(dx + x), (uint16_t)(dy + y), color);
        }
    }
}

} // namespace nema::shell
