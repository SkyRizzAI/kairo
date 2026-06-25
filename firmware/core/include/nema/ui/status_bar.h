#pragma once
#include <cstdint>

namespace nema {

class Canvas;

struct StatusBarData {
    int  hour    = 0;
    int  minute  = 0;
    int  battery = 100;   // 0–100
    bool wifi    = false;
    bool ble     = false;
    bool sdMounted  = false;
    bool locked  = false;
    bool visible = true;  // Plan 81: global Status Bar ON/OFF (display/statusbar)
    const char* version = "v0.1";
};

class StatusBar {
public:
    // Draw status bar at top of canvas + separator line below it
    static void draw(Canvas& c, const StatusBarData& d);

private:
    // Simplified hand-drawn battery (Plan 92 Fase D) — ~11px, fill ∝ pct.
    // `on` = draw polarity (false on a filled/inverted bar).
    static void drawBattery(Canvas& c, uint16_t x, uint16_t y, int pct, bool on);
    // Tiny 3×5 clock (HH:MM) — smaller than the 8px bitmap fonts (Plan 92 Fase D).
    static void drawClock(Canvas& c, uint16_t x, uint16_t y, int h, int m, bool on);
    // WiFi as standing ascending signal bars (7×5) — tidy at the bar height (Plan 92 Fase D).
    static void drawWifi(Canvas& c, uint16_t x, uint16_t y, bool on);
};

} // namespace nema
