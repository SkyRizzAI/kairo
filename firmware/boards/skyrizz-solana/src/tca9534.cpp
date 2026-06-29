#include "nema/skyrizzsolana/tca9534.h"
#include "nema/skyrizzsolana/board_config.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/input/i_key_map.h"
#include <Wire.h>
#include <Arduino.h>
#include <driver/gpio.h>

namespace nema::skyrizzsolana {

void Tca9534::isrHandler(void* arg) {
    auto* self = static_cast<Tca9534*>(arg);
    self->intFlag_ = true;
}

void Tca9534::init(Runtime& rt) {
    rt_ = &rt;
}

void Tca9534::start() {
    // All six button pins are inputs; P6/P7 unused → also inputs (1 = input).
    Wire.beginTransmission(I2C_ADDR_TCA9534);
    Wire.write(TCA9534_REG_CONFIG);
    Wire.write(0xFF);   // every bit an input
    Wire.endTransmission();

    // No polarity inversion — we invert in software (buttons are active-LOW).
    Wire.beginTransmission(I2C_ADDR_TCA9534);
    Wire.write(TCA9534_REG_POLARITY);
    Wire.write(0x00);
    Wire.endTransmission();

    // Seed last_ from the current state so we don't emit a spurious press on the
    // first tick (buttons idle HIGH → 0 after invert+mask).
    lastInputs_ = (uint8_t)(~readInputs() & PB_MASK);

    // INT# on GPIO4, falling edge (TCA9534 pulls low on any input change).
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = (1ULL << PIN_PB_INT);
    cfg.mode         = GPIO_MODE_INPUT;
    cfg.pull_up_en   = GPIO_PULLUP_ENABLE;
    cfg.intr_type    = GPIO_INTR_NEGEDGE;
    gpio_config(&cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add((gpio_num_t)PIN_PB_INT, &Tca9534::isrHandler, this);

    if (rt_) rt_->log().info("Tca9534", "started", {{"addr", "0x20"}, {"int", "GPIO4"}});
}

void Tca9534::stop() {
    gpio_isr_handler_remove((gpio_num_t)PIN_PB_INT);
}

void Tca9534::tick(uint64_t nowMs) {
    // Re-read on interrupt (instant press/release) OR every 15 ms. The periodic
    // poll is essential: a HELD button produces no edge → no interrupt, so
    // without it long-press / repeat could never fire.
    bool due = intFlag_ || (nowMs - lastPoll_ >= 15);
    if (due) {
        intFlag_  = false;
        lastPoll_ = nowMs;

        uint8_t inputs  = (uint8_t)(~readInputs() & PB_MASK);   // active-HIGH, masked
        uint8_t changed = (uint8_t)(inputs ^ lastInputs_);
        lastInputs_ = inputs;

        if (changed && keyMap_) {
            // feedButtons: wire each PB mask to a logical button id. Swap the
            // mask↔id pairing here if a button reads "wrong" at bring-up.
            auto feedBtn = [&](uint8_t btnId, uint8_t mask) {
                if (changed & mask) keyMap_->feedEdge(btnId, (inputs & mask) != 0, nowMs);
            };
            feedBtn(BTN_LEFT,  P_PB1);
            feedBtn(BTN_DOWN,  P_PB2);
            feedBtn(BTN_UP,    P_PB3);
            feedBtn(BTN_RIGHT, P_PB4);
            feedBtn(BTN_OK,    P_PB5);
            feedBtn(BTN_BACK,  P_PB6);
        }
    }

    // ALWAYS advance the gesture engine so long-press / repeat fire while held.
    if (keyMap_) keyMap_->tick(nowMs);
}

uint8_t Tca9534::readInputs() {
    Wire.beginTransmission(I2C_ADDR_TCA9534);
    Wire.write(TCA9534_REG_INPUT);
    Wire.endTransmission(false);   // repeated start
    Wire.requestFrom((uint8_t)I2C_ADDR_TCA9534, (uint8_t)1);
    return Wire.available() ? (uint8_t)Wire.read() : 0xFF;   // idle HIGH on no-read
}

} // namespace nema::skyrizzsolana
