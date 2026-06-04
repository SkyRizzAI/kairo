#include "kairo/skyrizze32/xl9535.h"
#include "kairo/skyrizze32/board_config.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/input/i_key_map.h"
#include <Wire.h>
#include <Arduino.h>
#include <driver/gpio.h>

namespace kairo::skyrizze32 {

void Xl9535::isrHandler(void* arg) {
    auto* self = static_cast<Xl9535*>(arg);
    self->intFlag_ = true;
}

void Xl9535::init(Runtime& rt) {
    rt_ = &rt;
}

void Xl9535::start() {
    // Configure port directions:
    //   Port 0: P00-P03 = output (backlight, resets); P04-P07 = input (buttons, ext)
    //   Port 1: P17 = output (LED); P10-P16 = input
    Wire.beginTransmission(I2C_ADDR_XL9535);
    Wire.write(XL9535_REG_CONFIG0);
    Wire.write(0b11110000);   // 0=output: P00-P03; 1=input: P04-P07
    Wire.endTransmission();

    Wire.beginTransmission(I2C_ADDR_XL9535);
    Wire.write(XL9535_REG_CONFIG1);
    Wire.write(0b01111111);   // 0=output: P17; 1=input: P10-P16
    Wire.endTransmission();

    // Initial output state: backlight ON, resets deasserted, LED off
    out0_ = P0_LCD_BLK | P0_TS_RST | P0_CAM_RST | P0_SE_RST;
    out1_ = 0;
    writeOutput(0, out0_);
    writeOutput(1, out1_);

    // Read initial input state to seed last_ (avoids spurious events on first tick)
    lastInputs_ = readRaw() ^ 0xFFFF;   // invert: active-HIGH after XOR
    lastInputs_ &= 0x7FF0;              // mask output bits out

    // Attach INT# ISR — GPIO43, falling edge (XL9535 pulls low on change)
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = (1ULL << PIN_BUS_INT);
    cfg.mode         = GPIO_MODE_INPUT;
    cfg.pull_up_en   = GPIO_PULLUP_ENABLE;
    cfg.intr_type    = GPIO_INTR_NEGEDGE;
    gpio_config(&cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add((gpio_num_t)PIN_BUS_INT, &Xl9535::isrHandler, this);

    if (rt_) rt_->log().info("Xl9535", "started (16-bit expander, INT on GPIO43)");
}

void Xl9535::stop() {
    gpio_isr_handler_remove((gpio_num_t)PIN_BUS_INT);
    setBacklight(false);
    setIndicatorLed(false);
}

void Xl9535::tick(uint64_t nowMs) {
    // Re-read inputs on interrupt (instant press/release) OR every 15 ms.
    // The periodic poll is essential: while a button is HELD steady there is
    // no edge → no interrupt, so without it long-press could never be detected.
    bool due = intFlag_ || (nowMs - lastPoll_ >= 15);
    if (due) {
        intFlag_  = false;
        lastPoll_ = nowMs;

        uint16_t raw     = readRaw();
        uint16_t inputs  = ~raw & 0x7FF0;    // invert active-LOW, mask output bits
        uint16_t changed = inputs ^ lastInputs_;
        lastInputs_ = inputs;

        if (changed && keyMap_) {
            auto feedBtn = [&](uint8_t btnId, uint16_t mask) {
                if (changed & mask) {
                    bool pressed = (inputs & mask) != 0;
                    keyMap_->feedEdge(btnId, pressed, nowMs);
                }
            };
            feedBtn(BTN_LEFT,   (uint16_t)(P1_SW1 << 8));   // SW1 = P12 (combined bit 10)
            feedBtn(BTN_MIDDLE, (uint16_t)P0_SW2);          // SW2 = P04 (combined bit 4)
            feedBtn(BTN_RIGHT,  (uint16_t)(P1_SW3 << 8));   // SW3 = P11 (combined bit 9)
            feedBtn(BTN_UP,     (uint16_t)P0_PB2);          // PB2 = P06 (combined bit 6) — top button
            feedBtn(BTN_DOWN,   (uint16_t)P0_PB1);          // PB1 = P05 (combined bit 5) — bottom button
        }
    }

    // ALWAYS advance the gesture engine so long-press / repeat fire while held,
    // even on ticks where we didn't touch I2C.
    if (keyMap_) keyMap_->tick(nowMs);
}

void Xl9535::setKeyMap(input::IKeyMap* km, uint64_t /*nowMs*/) {
    keyMap_ = km;
}

void Xl9535::setBacklight(bool on) {
    if (on) out0_ |= P0_LCD_BLK; else out0_ &= ~P0_LCD_BLK;
    writeOutput(0, out0_);
}

void Xl9535::setIndicatorLed(bool on) {
    if (on) out1_ |= P1_IND_LED; else out1_ &= ~P1_IND_LED;
    writeOutput(1, out1_);
}

void Xl9535::setTouchReset(bool asserted) {
    if (!asserted) out0_ |= P0_TS_RST; else out0_ &= ~P0_TS_RST;
    writeOutput(0, out0_);
}

void Xl9535::setCamReset(bool asserted) {
    if (!asserted) out0_ |= P0_CAM_RST; else out0_ &= ~P0_CAM_RST;
    writeOutput(0, out0_);
}

void Xl9535::setSeReset(bool asserted) {
    if (!asserted) out0_ |= P0_SE_RST; else out0_ &= ~P0_SE_RST;
    writeOutput(0, out0_);
}

uint16_t Xl9535::readInputs() {
    return ~readRaw() & 0x7FF0;   // active-HIGH, output bits masked
}

void Xl9535::writeOutput(uint8_t port, uint8_t val) {
    uint8_t reg = (port == 0) ? XL9535_REG_OUTPUT0 : XL9535_REG_OUTPUT1;
    Wire.beginTransmission(I2C_ADDR_XL9535);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

uint16_t Xl9535::readRaw() {
    Wire.beginTransmission(I2C_ADDR_XL9535);
    Wire.write(XL9535_REG_INPUT0);
    Wire.endTransmission(false);   // repeated start
    Wire.requestFrom((uint8_t)I2C_ADDR_XL9535, (uint8_t)2);
    uint8_t p0 = Wire.read();
    uint8_t p1 = Wire.read();
    return (uint16_t)((p1 << 8) | p0);
}

} // namespace kairo::skyrizze32
