// canvas-demo-wasm — Plan 86 Example (G5)
// Raw canvas drawing. All dimensions from canvas_width/height — no hardcoded values.
// Press any button to advance frame; Back to exit.
#include "nema_api.h"

NEMA_EXPORT int main(void) {
    int w = canvas_width();
    int h = canvas_height();
    int frame = 0;

    while (1) {
        canvas_clear(COLOR_BG);

        // Animated shrinking border
        int m = 2 + (frame % 8);
        canvas_rect(m, m, w - m * 2, h - m * 2, COLOR_FG);

        // Diagonal cross
        canvas_line(m, m, w - m - 1, h - m - 1, COLOR_FG);
        canvas_line(w - m - 1, m, m, h - m - 1, COLOR_FG);

        // Title
        canvas_text(2, 2, "Canvas Demo", COLOR_FG);

        // Frame counter (bottom-right)
        char fbuf[8];
        nema_itoa(frame, fbuf);
        canvas_text(w - 20, h - 10, fbuf, COLOR_FG);

        canvas_flush();

        int act = input_wait(0);
        if (act == ACT_BACK) break;
        frame++;
    }
    return 0;
}
