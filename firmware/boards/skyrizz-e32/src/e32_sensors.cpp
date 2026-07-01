#include "nema/skyrizze32/e32_sensors.h"
#include <Wire.h>
#include <Arduino.h>

namespace nema::skyrizze32 {

static constexpr uint8_t LTR303_ADDR = 0x29;
static constexpr uint8_t SC7A20_ADDR = 0x19;

static bool i2cPresent(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

static bool i2cReadReg(uint8_t addr, uint8_t reg, uint8_t* out, uint8_t n) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(addr, n) != n) return false;
    for (uint8_t i = 0; i < n; i++) out[i] = Wire.read();
    return true;
}

static void i2cWriteReg(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

// ── LTR-303ALS ──────────────────────────────────────────────────────────────
bool Ltr303::begin() {
    if (!i2cPresent(LTR303_ADDR)) { present_ = false; return false; }
    i2cWriteReg(LTR303_ADDR, 0x80, 0x01);   // ALS_CONTR: active mode, gain 1x
    delay(10);                              // wake-up time
    present_ = true;
    return true;
}

bool Ltr303::read() {
    if (!present_) return false;
    uint8_t d[4];
    // 0x88=CH1_0, 0x89=CH1_1, 0x8A=CH0_0, 0x8B=CH0_1 (read CH1 then CH0 order).
    if (!i2cReadReg(LTR303_ADDR, 0x88, d, 4)) return false;
    uint16_t ch0 = (uint16_t)(d[2] | (d[3] << 8));   // visible + IR
    lux_ = (float)ch0;   // approx; precise lux needs the CH0/CH1 ratio formula
    return true;
}

// ── SC7A20 (LIS2DH-compatible) ───────────────────────────────────────────────
bool Sc7a20::begin() {
    if (!i2cPresent(SC7A20_ADDR)) { present_ = false; return false; }
    i2cWriteReg(SC7A20_ADDR, 0x20, 0x57);   // CTRL_REG1: 100 Hz, X/Y/Z enable
    present_ = true;
    return true;
}

bool Sc7a20::read() {
    if (!present_) return false;
    uint8_t d[6];
    // Auto-increment (0x80) from OUT_X_L (0x28). ±2g, 16-bit left-justified.
    if (!i2cReadReg(SC7A20_ADDR, 0x28 | 0x80, d, 6)) return false;
    int16_t rx = (int16_t)(d[0] | (d[1] << 8));
    int16_t ry = (int16_t)(d[2] | (d[3] << 8));
    int16_t rz = (int16_t)(d[4] | (d[5] << 8));
    x_ = rx / 16384.0f;   // 16384 LSB/g at ±2g full 16-bit
    y_ = ry / 16384.0f;
    z_ = rz / 16384.0f;
    return true;
}

} // namespace nema::skyrizze32
