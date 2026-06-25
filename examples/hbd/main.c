// HBD — Happy Birthday cake (Palanu WASM example, raw canvas)
//
// A staged birthday scene:
//   1. a floor line wipes in,
//   2. the cake drops from mid-air onto the floor,
//   3. the candles light (flickering flames),
//   4. "HAPPY BIRTHDAY!!!" floats up from the cake to the top, then blinks,
//   5. a "Blow out the candles" prompt + a Blow button appear.
// Press OK to blow: flames → smoke, three balloons drift up off-screen, and the
// prompt becomes "Make a wish!". Press OK again for a celebratory finale
// (sparkles + fireworks). OK there replays; Back exits anytime.
//
// 1-bit display: "opacity" is approximated by temporal dithering (fade) and
// on/off blinking. Everything derives from canvas_width()/height().
#include "nema_api.h"

#define N_CANDLES 3
#define FRAME_MS  40            // ~25 fps animation tick

enum { SC_LIT, SC_BLOWN, SC_WISH };

// timeline beats for the LIT intro (ms since the scene started)
#define T_CAKE_IN   500         // cake starts dropping
#define T_SEATED    1500        // cake on floor → flames + title begin
#define T_TITLE_TOP 2500        // title reached the top
#define T_TITLE_BLINK 3500      // title starts blinking
#define T_HINT      4500        // "Blow out the candles" prompt
#define T_READY     5500        // Blow button appears (OK now blows)

// ── "Happy Birthday to You" melody (looped throughout the app) ─────────────────
// One note is fired per onset from the frame loop (non-blocking on the simulator),
// so the tune plays while the animation runs and never blocks input.
enum { R = 0, G4 = 392, A4 = 440, B4 = 494, C5 = 523,
       D5 = 587, E5 = 659, F5 = 698, G5 = 784 };       // note freqs (Hz); R = rest
enum { DE = 200, DQ = 380, DH = 720 };                 // eighth / quarter / half (ms)
static const struct { int f, ms; } SONG[] = {
    {G4,DE},{G4,DE},{A4,DQ},{G4,DQ},{C5,DQ},{B4,DH},            // Happy birth-day to you
    {G4,DE},{G4,DE},{A4,DQ},{G4,DQ},{D5,DQ},{C5,DH},            // Happy birth-day to you
    {G4,DE},{G4,DE},{G5,DQ},{E5,DQ},{C5,DQ},{B4,DQ},{A4,DH},    // Happy birth-day dear ...
    {F5,DE},{F5,DE},{E5,DQ},{C5,DQ},{D5,DQ},{C5,DH},            // Happy birth-day to you
    {R, 800},                                                   // pause, then loop
};
static const int SONG_N = (int)(sizeof(SONG) / sizeof(SONG[0]));

// ── small helpers ─────────────────────────────────────────────────────────────
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// deterministic pseudo-random in [0,m) from a seed (no libc rand).
static int prand(int seed, int m) {
    unsigned int x = (unsigned int)seed * 1103515245u + 12345u;
    x ^= x >> 13;
    return (int)((x >> 8) % (unsigned int)m);
}

static void text_center(int cx, int y, const char* s) {
    canvas_text(cx - (nema_strlen(s) * 6) / 2, y, s, COLOR_FG);
}

// Filled "pill" button with a 1-px corner cut and a centred, inverted label.
static void draw_button(int cx, int y, const char* label) {
    const int charW = 6, charH = 8, padX = 6, padY = 3;
    int tw = nema_strlen(label) * charW;
    int bw = tw + padX * 2;
    int bh = charH + padY * 2;
    int bx = cx - bw / 2;
    canvas_fill_rect(bx, y, bw, bh, COLOR_FG);
    canvas_pixel(bx,          y,          COLOR_BG);
    canvas_pixel(bx + bw - 1, y,          COLOR_BG);
    canvas_pixel(bx,          y + bh - 1, COLOR_BG);
    canvas_pixel(bx + bw - 1, y + bh - 1, COLOR_BG);
    canvas_text(cx - tw / 2, y + padY, label, COLOR_BG);
}

// ── floor / ground ─────────────────────────────────────────────────────────────
// Wipes in from the centre outward as frac1000 goes 0→1000.
static void draw_floor(int cx, int floorY, int w, int frac1000) {
    int half = ((w / 2) - 4) * frac1000 / 1000;
    if (half < 1) half = 1;
    canvas_fill_rect(cx - half, floorY, half * 2, 1, COLOR_FG);
    for (int x = cx - half + 2; x < cx + half; x += 6)   // little ground ticks
        canvas_pixel(x, floorY + 2, COLOR_FG);
}

// ── cake ──────────────────────────────────────────────────────────────────────
static void draw_cake(int cx, int plateY, int cakeW, int bodyH,
                      int candleX[N_CANDLES], int* tierTopY) {
    int bodyX = cx - cakeW / 2;
    int bodyY = plateY - bodyH;

    int plateW = cakeW + cakeW / 3;
    canvas_fill_rect(cx - plateW / 2, plateY, plateW, 2, COLOR_FG);
    canvas_line(cx - plateW / 2 - 2, plateY + 2, cx + plateW / 2 + 1, plateY + 2, COLOR_FG);

    canvas_rect(bodyX, bodyY, cakeW, bodyH, COLOR_FG);              // hollow body

    int icingH = bodyH / 3;
    canvas_fill_rect(bodyX, bodyY, cakeW, icingH, COLOR_FG);        // icing band
    int dripW = clampi(cakeW / 7, 4, 12);
    for (int x = 1; x < cakeW - 1; x++) {
        int p = x % dripW, half = dripW / 2;
        int depth = (p < half) ? p : (dripW - 1 - p);              // rounded drips
        canvas_fill_rect(bodyX + x, bodyY + icingH, 1, depth + 1, COLOR_FG);
    }
    int decY = bodyY + icingH + (bodyH - icingH) * 2 / 3;          // sprinkles
    for (int x = bodyX + 4; x < bodyX + cakeW - 3; x += 6)
        canvas_fill_rect(x, decY, 2, 1, COLOR_FG);

    *tierTopY = bodyY;
    int margin = cakeW / 6, span = cakeW - 2 * margin;
    for (int i = 0; i < N_CANDLES; i++)
        candleX[i] = bodyX + margin + (N_CANDLES > 1 ? span * i / (N_CANDLES - 1) : span / 2);
}

static void draw_candle(int cxc, int topY, int candleW, int candleH, int* wickY) {
    int x = cxc - candleW / 2, y = topY - candleH;
    canvas_fill_rect(x, y, candleW, candleH, COLOR_FG);
    for (int r = 0; r < candleH; r += 3)
        canvas_pixel(x + (r % candleW), y + r, COLOR_BG);          // stripes
    canvas_fill_rect(cxc, y - 2, 1, 2, COLOR_FG);                  // wick
    *wickY = y - 2;
}

static void draw_flame(int cxc, int wickY, int phase) {
    static const int hgt[4]  = {3, 4, 3, 5};
    static const int sway[4] = {0, 1, 0, -1};
    int fh = hgt[phase & 3], sx = sway[phase & 3];
    int tipY = wickY - fh;
    for (int k = 0; k < fh; k++) {
        int half = (k * 2) / fh, yy = tipY + k + 1;
        canvas_fill_rect(cxc + sx - half, yy, half * 2 + 1, 1, COLOR_FG);
    }
    canvas_pixel(cxc + sx, tipY, COLOR_FG);
}

// Short wisp that rises from the wick and thins to nothing, then loops.
static void draw_smoke(int cxc, int wickY, int t, int maxRise) {
    static const int wig[8] = {0, 0, 1, 1, 0, -1, -1, 0};
    const int L = 5;
    int cycle = clampi(maxRise, 12, 26);
    int rise  = t % cycle;
    int visible = L * (cycle - rise) / cycle;
    for (int k = 0; k < visible; k++) {
        int yy = wickY - rise - k;
        if (yy < 1) break;
        canvas_pixel(cxc + wig[(rise + k) % 8], yy, COLOR_FG);
    }
}

// ── balloon (filled oval + shine + wavy string) ────────────────────────────────
static void draw_balloon(int cxb, int topY, int bw, int frame) {
    int bh = bw + bw / 3;
    for (int j = 0; j < bh; j++) {
        int rel = j * 100 / bh, wj;
        if (rel < 15)      wj = bw * rel / 15;            // round top
        else if (rel > 75) wj = bw * (100 - rel) / 25;    // taper to knot
        else               wj = bw;
        if (wj < 1) wj = 1;
        canvas_fill_rect(cxb - wj / 2, topY + j, wj, 1, COLOR_FG);
    }
    canvas_pixel(cxb - bw / 4,     topY + bh / 4, COLOR_BG);   // shine
    canvas_pixel(cxb - bw / 4 + 1, topY + bh / 4, COLOR_BG);
    canvas_fill_rect(cxb, topY + bh, 1, 2, COLOR_FG);          // knot
    static const int sw[4] = {0, 1, 0, -1};
    for (int k = 0; k < bw; k++)                               // wavy string
        canvas_pixel(cxb + sw[(k + frame / 2) % 4], topY + bh + 2 + k, COLOR_FG);
}

// Three balloons drifting up off-screen over ~3s (t = ms since blow).
static void draw_balloons(int w, int h, int t) {
    int xs[3]  = { w / 4, w / 2, (3 * w) / 4 };
    int off[3] = { 0, 600, 300 };                       // staggered launches
    int bw = clampi(w / 14, 6, 12);
    int span = bw + bw / 3 + bw + 6;                    // body + string + margin
    int travel = h + span;                              // bottom edge → fully off top
    for (int i = 0; i < 3; i++) {
        int ti = t - off[i];
        if (ti < 0) continue;
        int topY = (h + 4) - travel * ti / 3000;        // rises bottom → top
        if (topY > h || topY < -span) continue;         // not yet / already gone
        int sway = ((ti / 120 + i) & 1) ? 1 : -1;       // gentle horizontal sway
        draw_balloon(xs[i] + sway, topY, bw, ti / FRAME_MS);
    }
}

// ── sparkles + fireworks for the finale ───────────────────────────────────────
static void draw_sparkles(int w, int h, int fc) {
    for (int i = 0; i < 26; i++) {
        int sx = prand(i * 2 + 1, w);
        int sy = prand(i * 2 + 99, h);
        if (((i * 7 + fc / 2) % 5) >= 2) continue;        // twinkle on/off
        canvas_pixel(sx, sy, COLOR_FG);
        if ((i + fc / 3) % 3 == 0) {                       // bigger star: a plus
            canvas_pixel(sx - 1, sy, COLOR_FG); canvas_pixel(sx + 1, sy, COLOR_FG);
            canvas_pixel(sx, sy - 1, COLOR_FG); canvas_pixel(sx, sy + 1, COLOR_FG);
        }
    }
}
static void draw_firework(int fx, int fy, int phase) {
    static const int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    static const int dy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    int r = phase % 12;                                    // expanding ring
    if (r >= 10) return;                                   // brief gap between bursts
    for (int d = 0; d < 8; d++) {
        int x = fx + dx[d] * r, y = fy + dy[d] * r;
        canvas_pixel(x, y, COLOR_FG);
        if (r > 3) canvas_pixel(x - dx[d], y - dy[d], COLOR_FG);  // trailing spark
    }
}

NEMA_EXPORT int main(void) {
    int w = canvas_width();
    int h = canvas_height();
    int cx = w / 2;

    int cakeW   = clampi(w * 11 / 20, 40, w - 8);
    int bodyH   = clampi(h * 20 / 100, 16, h / 3);
    int candleW = clampi(cakeW / 14, 2, 6);
    int candleH = clampi(h / 9, 8, h / 5);

    // Title + prompt sit at the top. The cake + floor own the lower part of the
    // screen, and the Blow/OK button FLOATS just above the cake (down in the lower
    // area, hovering over the cake top — not crammed at the very bottom edge).
    //
    // Reserve the system status-bar strip: a Normal-mode app has the status bar
    // overlaid on its top ~14 logical px (= CONTENT_Y), so the title must start at
    // or below that or its top is hidden behind the bar. 14px is the bar height in
    // LOGICAL pixels — it's already UI-scale-relative, so this holds at any scale.
    int kStatusBar = 14;
    int titleTopY  = clampi(h / 12, kStatusBar, kStatusBar + 6);   // below the status bar
    int hintY      = titleTopY + 12;          // prompt right under the title
    int floorY     = h - 6;                   // ground near the bottom (cake gets the space)
    int seatPlateY = floorY - 1;              // cake's resting plate Y
    // Flame-top of the seated cake — title rises up from here.
    int flamesTop  = seatPlateY - bodyH - candleH - 6;
    // Button: bottom-centre, floating low over the cake. "Above the cake" = drawn
    // on a LAYER on top of it (rendered after the cake), not lifted up in Y — so it
    // hovers over the cake near the floor and needs no dedicated bottom strip.
    int btnY       = floorY - 16;
    // How far the cake falls onto the floor (ease-out drop).
    int headroom   = seatPlateY - bodyH - candleH - 12;
    int dropDist   = clampi(h / 4, 4, headroom > 4 ? headroom : 4);
    int startPlateY = seatPlateY - dropDist;
    int titleMidY  = clampi(flamesTop, hintY + 8, h * 3 / 5);  // title rises from the cake top

    int scene = SC_LIT;
    int tms = 0, fc = 0;

    audio_set_volume(75);
    int sNote = 0, sLeft = 0;   // melody cursor + ms left in the current note

    while (1) {
        // Melody: fire the next note at its onset; it plays async while we animate.
        if (sLeft <= 0) {
            int f = SONG[sNote].f, ms = SONG[sNote].ms;
            if (f > 0) audio_play_tone(f, ms * 4 / 5);   // 80% on → notes stay detached
            sLeft = ms;
            sNote = (sNote + 1) % SONG_N;
        }
        sLeft -= FRAME_MS;

        canvas_clear(COLOR_BG);

        if (scene == SC_LIT) {
            int ffrac = (tms < T_CAKE_IN) ? (tms * 1000 / T_CAKE_IN) : 1000;
            draw_floor(cx, floorY, w, ffrac);

            if (tms >= T_CAKE_IN) {
                int dp = clampi((tms - T_CAKE_IN) * 1000 / (T_SEATED - T_CAKE_IN), 0, 1000);
                int de = 1000 - (1000 - dp) * (1000 - dp) / 1000;          // ease-out drop
                int plateY = startPlateY + (seatPlateY - startPlateY) * de / 1000;

                int candleX[N_CANDLES], tierTopY;
                draw_cake(cx, plateY, cakeW, bodyH, candleX, &tierTopY);

                int seated = (tms >= T_SEATED);
                for (int i = 0; i < N_CANDLES; i++) {
                    int wickY;
                    draw_candle(candleX[i], tierTopY, candleW, candleH, &wickY);
                    if (seated) draw_flame(candleX[i], wickY, fc / 5 + i);       // calm flicker
                }

                // Title: floats up SOLID from the cake to the top, holds 1s, then blinks.
                int show = 1, ty = titleTopY;
                if (tms >= T_SEATED && tms < T_TITLE_TOP) {
                    int p = (tms - T_SEATED) * 1000 / (T_TITLE_TOP - T_SEATED);  // 0..1000
                    ty = titleMidY + (titleTopY - titleMidY) * p / 1000;        // rising
                } else if (tms >= T_TITLE_BLINK) {
                    show = ((tms - T_TITLE_BLINK) / 250) % 2 == 0;              // blink 0,1,0,1
                }
                if (tms >= T_SEATED && show) text_center(cx, ty, "HAPPY BIRTHDAY!!!");

                if (tms >= T_HINT)  text_center(cx, hintY, "Blow the candles");
                if (tms >= T_READY) draw_button(cx, btnY, "Blow");
            }

        } else if (scene == SC_BLOWN) {
            draw_floor(cx, floorY, w, 1000);
            int candleX[N_CANDLES], tierTopY;
            draw_cake(cx, seatPlateY, cakeW, bodyH, candleX, &tierTopY);
            for (int i = 0; i < N_CANDLES; i++) {
                int wickY;
                draw_candle(candleX[i], tierTopY, candleW, candleH, &wickY);
                draw_smoke(candleX[i], wickY, (tms / FRAME_MS) + i * 7,
                           tierTopY - candleH - 4);
            }
            text_center(cx, titleTopY, "HAPPY BIRTHDAY!!!");
            draw_balloons(w, h, tms);                      // float up off-screen (~3s)
            text_center(cx, hintY, "Make a wish!");
            draw_button(cx, btnY, "OK");

        } else { // SC_WISH — celebratory finale
            draw_sparkles(w, h, fc);
            draw_firework(w / 4,     h / 3,     fc);
            draw_firework((3 * w) / 4, h / 4,   fc + 6);
            draw_firework(cx,        h / 2,     fc + 3);
            int blink = (tms / 350) % 2 == 0;
            if (blink) text_center(cx, h / 3 - 6, "HAPPY BIRTHDAY!!!");
            text_center(cx, h / 3 + 8, "Wish granted!");
            text_center(cx, h - 11, "OK: again");
        }

        canvas_flush();

        int act = input_poll();
        if (act == ACT_BACK) break;
        if (act == ACT_ACTIVATE) {
            if (scene == SC_LIT) {
                if (tms < T_READY) tms = T_READY;          // OK skips the intro
                else { scene = SC_BLOWN; tms = 0; fc = 0; }
            } else if (scene == SC_BLOWN) {
                scene = SC_WISH; tms = 0; fc = 0;
            } else { // SC_WISH
                scene = SC_LIT; tms = 0; fc = 0;           // replay
            }
        }

        delay(FRAME_MS);
        tms += FRAME_MS;
        fc++;
    }
    return 0;
}
