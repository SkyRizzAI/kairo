#pragma once
#include <cstdint>

namespace kairo {

class Canvas;

// Shared UI widgets. Status bar lives in status_bar.*.
namespace ui {

// Draw a centered screen title with breathing room below the status bar and an
// underline separator, then return the y where body content should begin.
// Use this for every titled screen so the header padding is consistent.
uint16_t drawTitle(Canvas& c, const char* title);

// Draw a centered modal box: clears a white rectangle and outlines it.
// Returns nothing — caller draws content inside using the same coords.
// (mx,my) of the box = ((W-w)/2, (H-h)/2). Use modalOriginX/Y to locate it.
void drawModalBox(Canvas& c, uint16_t w, uint16_t h);
uint16_t modalOriginX(const Canvas& c, uint16_t w);
uint16_t modalOriginY(const Canvas& c, uint16_t h);

// Draw a full Yes/No confirmation modal over whatever is already on the canvas.
// prompt: question text. cursor: 0 = Yes highlighted, 1 = No highlighted.
// App-model friendly: an app calls this on its own buffer (no ViewDispatcher).
void drawConfirm(Canvas& c, const char* prompt, int cursor,
                 uint16_t w = 210, uint16_t h = 56);

} // namespace ui
} // namespace kairo
