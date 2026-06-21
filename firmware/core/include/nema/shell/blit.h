#pragma once
// Plan 81 — shared 1-bit blit helpers for the shell skins.
#include "nema/shell/desktop_theme.h"   // FitMode / Anchor
#include <cstdint>

namespace nema { class Canvas; }

namespace nema::shell {

// Wallpaper blit: scales a 1-bit frame into a target rect honouring fit+anchor and
// draws BOTH on- and off-pixels (fully covers the fitted area). Frame bits are a
// continuous MSB-first stream (bit = row*sw + col), as Canvas::drawBitmap expects.
void blitFit(nema::Canvas& c, const uint8_t* bits, int sw, int sh,
             int tx, int ty, int tw, int th, FitMode fit, Anchor anchor);

// Icon blit: stretches a 1-bit bitmap to dw×dh (nearest-neighbour) and draws ONLY
// the source-on pixels in `color` — off pixels are transparent. Use to scale small
// icons up to fill a tile, in black (color=false) or white (color=true).
void blitScaledMask(nema::Canvas& c, const uint8_t* bits, int sw, int sh,
                    int dx, int dy, int dw, int dh, bool color);

} // namespace nema::shell
