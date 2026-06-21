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
    static void drawBattery(Canvas& c, uint16_t x, uint16_t y, int pct);
    static void drawClock  (Canvas& c, uint16_t x, uint16_t y, int h, int m);
};

} // namespace nema
