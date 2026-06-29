#include "nema/skyrizzsolana/touch_panel.h"
#include "nema/skyrizzsolana/board_config.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/service/service_container.h"
#include "nema/config/config_store.h"
#include <Wire.h>
#include <Arduino.h>

namespace nema::skyrizzsolana {

// Logical panel size (matches LcdDriver default; portrait 240×320, scale 1).
static constexpr uint16_t LCD_W = 240;
static constexpr uint16_t LCD_H = 320;

// ── FT6336U (capacitive) registers ──────────────────────────────────────────
static constexpr uint8_t FT_TD_STATUS = 0x02;
static constexpr uint8_t FT_TOUCH1_XH  = 0x03;   // XH,XL,YH,YL = 0x03..0x06

// ── TSC2007 (resistive) command set (Adafruit-compatible) ───────────────────
// cmd = (func << 4) | (power << 2) | mode.  func: 0=MEASURE_X? — see below.
// We use 12-bit mode (mode=0), ADC-on/IRQ-off power (power=1) for measurements.
static constexpr uint8_t TSC_MEAS_X  = 0xC0;   // (12<<4)|(0<<2)|0 → measure X
static constexpr uint8_t TSC_MEAS_Y  = 0xD0;   // (13<<4)
static constexpr uint8_t TSC_MEAS_Z1 = 0xE0;   // (14<<4)
// Raw-ADC → pixel calibration window (tune at bring-up). Resistive panels never
// reach the full 0..4095 rail; these are typical edge values.
static constexpr uint16_t TSC_X_MIN = 150, TSC_X_MAX = 3950;
static constexpr uint16_t TSC_Y_MIN = 200, TSC_Y_MAX = 3900;
static constexpr uint16_t TSC_Z_TOUCH = 80;    // Z1 above this → finger down

void TouchPanel::init(Runtime& rt) {
    rt_ = &rt;
    if (auto* cfg = rt.container().resolve<IConfigStore>())
        rotation_ = (uint8_t)(cfg->getIntOr("display", "rotation", 0) & 3);
}

void TouchPanel::start() {
    // Shared touch-reset (GPIO6, direct, active-LOW): assert 50 ms → release → 400 ms.
    pinMode(PIN_TS_RST, OUTPUT);
    digitalWrite(PIN_TS_RST, LOW);   delay(50);
    digitalWrite(PIN_TS_RST, HIGH);  delay(400);
    pinMode(PIN_TS_INT, INPUT_PULLUP);   // PENIRQ, active-LOW; we poll for v1

    // Probe capacitive first, then resistive.
    Wire.beginTransmission(I2C_ADDR_FT6336);
    bool ftAck = (Wire.endTransmission() == 0);
    Wire.beginTransmission(I2C_ADDR_TSC2007);
    bool tscAck = (Wire.endTransmission() == 0);

    if (ftAck)       ctrl_ = Controller::Ft6336;
    else if (tscAck) ctrl_ = Controller::Tsc2007;
    else             ctrl_ = Controller::None;

    if (rt_) {
        switch (ctrl_) {
            case Controller::Ft6336:  rt_->log().info("TouchPanel", "FT6336U capacitive @0x38"); break;
            case Controller::Tsc2007: rt_->log().info("TouchPanel", "TSC2007 resistive @0x4A");  break;
            default: rt_->log().warn("TouchPanel", "no touch controller ACK (0x38/0x4A)");        break;
        }
    }
}

void TouchPanel::tick(uint64_t nowMs) {
    if (ctrl_ == Controller::None) return;
    if (nowMs - lastPoll_ < 8) return;   // ~125 Hz poll
    lastPoll_ = nowMs;

    uint16_t rx = 0, ry = 0;
    bool down = false;
    bool ok = (ctrl_ == Controller::Ft6336) ? readFt6336(rx, ry, down)
                                            : readTsc2007(rx, ry, down);
    if (!ok) return;

    if (down) {
        uint16_t lx, ly;
        toLogical(rx, ry, lx, ly);
        if (!wasDown_) {
            emitPointer(input::PointerPhase::Down, lx, ly);
            wasDown_ = true;
        } else if (lx != lastX_ || ly != lastY_) {
            emitPointer(input::PointerPhase::Move, lx, ly);
        }
        lastX_ = lx; lastY_ = ly;
    } else if (wasDown_) {
        emitPointer(input::PointerPhase::Up, lastX_, lastY_);
        wasDown_ = false;
    }
}

// ── FT6336U: read TD_STATUS, then touch-point 1 (panel coords already) ──────
bool TouchPanel::readFt6336(uint16_t& x, uint16_t& y, bool& down) {
    Wire.beginTransmission(I2C_ADDR_FT6336);
    Wire.write(FT_TD_STATUS);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)I2C_ADDR_FT6336, (uint8_t)1) != 1) return false;
    uint8_t points = Wire.read() & 0x0F;
    if (points == 0 || points > 2) { down = false; return true; }

    uint8_t d[4] = {0};
    Wire.beginTransmission(I2C_ADDR_FT6336);
    Wire.write(FT_TOUCH1_XH);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)I2C_ADDR_FT6336, (uint8_t)4) != 4) return false;
    for (int i = 0; i < 4; i++) d[i] = Wire.read();
    x = (uint16_t)(((d[0] & 0x0F) << 8) | d[1]);
    y = (uint16_t)(((d[2] & 0x0F) << 8) | d[3]);
    down = true;
    return true;
}

// ── TSC2007: read Z1 (pressure) then X/Y, scale raw ADC → panel pixels ──────
bool TouchPanel::readTsc2007(uint16_t& x, uint16_t& y, bool& down) {
    auto measure = [](uint8_t cmd, uint16_t& val) -> bool {
        Wire.beginTransmission(I2C_ADDR_TSC2007);
        Wire.write(cmd);
        if (Wire.endTransmission() != 0) return false;
        if (Wire.requestFrom((uint8_t)I2C_ADDR_TSC2007, (uint8_t)2) != 2) return false;
        uint8_t hi = Wire.read(), lo = Wire.read();
        val = (uint16_t)((hi << 4) | (lo >> 4));   // 12-bit
        return true;
    };

    uint16_t z1 = 0, rx = 0, ry = 0;
    if (!measure(TSC_MEAS_Z1, z1)) return false;
    if (z1 < TSC_Z_TOUCH) { down = false; return true; }   // no finger
    if (!measure(TSC_MEAS_X, rx)) return false;
    if (!measure(TSC_MEAS_Y, ry)) return false;

    // Clamp + scale raw ADC to panel pixels (calibration window above).
    auto scale = [](uint16_t v, uint16_t lo, uint16_t hi, uint16_t span) -> uint16_t {
        if (v <= lo) return 0;
        if (v >= hi) return (uint16_t)(span - 1);
        return (uint16_t)((uint32_t)(v - lo) * (span - 1) / (hi - lo));
    };
    x = scale(rx, TSC_X_MIN, TSC_X_MAX, LCD_W);
    y = scale(ry, TSC_Y_MIN, TSC_Y_MAX, LCD_H);
    down = true;
    return true;
}

// Raw panel (portrait 240×320) → logical canvas coords, applying display rotation
// (Plan 92 Fase A). Pairs with the LCD MADCTL set {0x48,0x28,0x88,0xE8}. If an
// orientation reads mirrored on real hardware, flip the sign in that one case.
void TouchPanel::toLogical(uint16_t rawX, uint16_t rawY, uint16_t& lx, uint16_t& ly) {
    if (rawX >= LCD_W) rawX = LCD_W - 1;
    if (rawY >= LCD_H) rawY = LCD_H - 1;
    switch (rotation_ & 3) {
        default:
        case 0: lx = rawX;               ly = rawY;               break; // 0°   240×320
        case 1: lx = rawY;               ly = (LCD_W - 1) - rawX; break; // 90°  320×240
        case 2: lx = (LCD_W - 1) - rawX; ly = (LCD_H - 1) - rawY; break; // 180° 240×320
        case 3: lx = (LCD_H - 1) - rawY; ly = rawX;               break; // 270° 320×240
    }
}

} // namespace nema::skyrizzsolana
