#include "nema/skyrizze32/ft6336_touch.h"
#include "nema/skyrizze32/xl9535.h"
#include "nema/skyrizze32/board_config.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include <Wire.h>
#include <Arduino.h>

namespace nema::skyrizze32 {

static constexpr uint8_t FT_ADDR        = 0x38;
static constexpr uint8_t FT_TD_STATUS   = 0x02;
static constexpr uint8_t FT_TOUCH1_XH   = 0x03;   // XH,XL,YH,YL = 0x03..0x06

// Logical panel size (matches LcdDriver default; portrait 240×320, scale 1).
static constexpr uint16_t LCD_W = 240;
static constexpr uint16_t LCD_H = 320;

void Ft6336Touch::init(Runtime& rt, Xl9535& expander) {
    rt_       = &rt;
    expander_ = &expander;
}

void Ft6336Touch::start() {
    // Reset pulse via XL9535 P01 (active-LOW): assert 50ms → release → 400ms.
    if (expander_) {
        expander_->setTouchReset(true);   delay(50);
        expander_->setTouchReset(false);  delay(400);
    }
    pinMode(PIN_TS_INT, INPUT_PULLUP);    // PENIRQ (active-LOW); we poll for v1

    // Presence probe
    Wire.beginTransmission(FT_ADDR);
    bool ack = (Wire.endTransmission() == 0);
    if (rt_) {
        if (ack) rt_->log().info ("Ft6336Touch", "FT6336U ACK at 0x38");
        else     rt_->log().warn ("Ft6336Touch", "FT6336U no ACK at 0x38");
    }
}

void Ft6336Touch::tick(uint64_t nowMs) {
    if (nowMs - lastPoll_ < 8) return;    // ~125 Hz poll — lower touch latency
    lastPoll_ = nowMs;

    uint16_t rx = 0, ry = 0;
    uint8_t  points = 0;
    if (!readPoint(rx, ry, points)) return;

    bool down = (points > 0 && points <= 2);

    if (down) {
        uint16_t lx, ly;
        toLogical(rx, ry, lx, ly);
        if (!wasDown_) {
            emitPointer(input::PointerPhase::Down, lx, ly);
            wasDown_ = true;
        } else if (lx != lastX_ || ly != lastY_) {
            emitPointer(input::PointerPhase::Move, lx, ly);  // live drag tracking
        }
        lastX_ = lx; lastY_ = ly;
    } else if (wasDown_) {
        emitPointer(input::PointerPhase::Up, lastX_, lastY_);
        wasDown_ = false;
    }
}

bool Ft6336Touch::readPoint(uint16_t& x, uint16_t& y, uint8_t& points) {
    uint8_t td = 0;
    Wire.beginTransmission(FT_ADDR);
    Wire.write(FT_TD_STATUS);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)FT_ADDR, (uint8_t)1) != 1) return false;
    td = Wire.read();
    points = td & 0x0F;
    if (points == 0 || points > 2) return true;   // valid read, just no touch

    uint8_t d[4] = {0};
    Wire.beginTransmission(FT_ADDR);
    Wire.write(FT_TOUCH1_XH);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)FT_ADDR, (uint8_t)4) != 4) return false;
    for (int i = 0; i < 4; i++) d[i] = Wire.read();
    x = (uint16_t)(((d[0] & 0x0F) << 8) | d[1]);
    y = (uint16_t)(((d[2] & 0x0F) << 8) | d[3]);
    return true;
}

// Raw panel → logical canvas coordinates. The FT6336U panel here is native
// portrait 240×320, matching the LCD's MADCTL 0x48, so the mapping is direct
// (clamp only). If touch axes turn out swapped/flipped at bring-up, correct the
// transform HERE — components never see raw values.
void Ft6336Touch::toLogical(uint16_t rawX, uint16_t rawY, uint16_t& lx, uint16_t& ly) {
    lx = rawX >= LCD_W ? (LCD_W - 1) : rawX;
    ly = rawY >= LCD_H ? (LCD_H - 1) : rawY;
}

} // namespace nema::skyrizze32
