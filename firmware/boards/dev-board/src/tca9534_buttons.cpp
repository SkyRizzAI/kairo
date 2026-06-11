#include "kairo/devboard/tca9534_buttons.h"
#include "kairo/devboard/board_config.h"
#include "kairo/devboard/dev_board_key_map.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/services/input_service.h"
#include "kairo/input/i_key_map.h"
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace kairo {

using namespace devboard;

void TCA9534Buttons::init(Runtime& rt) {
    rt_    = &rt;
    input_ = &rt.input();
    // keymap is already installed by DevBoard::describeHardware before init()
}

void TCA9534Buttons::start() {
    // Configure all TCA9534 pins as inputs
    Wire.beginTransmission(I2C_ADDR_TCA9534);
    Wire.write(TCA9534_REG_CONFIG);
    Wire.write(0xFF);  // all inputs
    Wire.endTransmission();
    last_ = readRaw();  // baseline — clears any pending INT

    // Dedicated polling thread — never blocked by display/app work.
    // Core 0 (with WiFi) is fine: an I²C read is ~1ms and the thread sleeps
    // 15ms between polls. Main loop runs on core 1.
    thread_.start({"btn_poll", 3072, 6, 0}, &TCA9534Buttons::pollThread, this);
    rt_->log().info("TCA9534Buttons", "started (6 buttons, poll thread)");
}

void TCA9534Buttons::stop() {
    thread_.requestStop();
    thread_.join();
}

void TCA9534Buttons::pollThread(void* arg) {
    auto* self = static_cast<TCA9534Buttons*>(arg);
    while (!self->thread_.shouldStop()) {
        self->pollOnce();
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

void TCA9534Buttons::pollOnce() {
    uint8_t btns    = readRaw();
    uint8_t changes = btns ^ last_;
    last_           = btns;

    // Get a monotonic timestamp for gesture engine (ms since boot).
    uint64_t now = (uint64_t)pdTICKS_TO_MS(xTaskGetTickCount());

    auto* km = input_->keyMap();

    if (km) {
        // Route through keymap: feed rising and falling edges for gesture detection.
        for (uint8_t b = 0; b < 6; b++) {
            uint8_t mask = 1 << b;
            if (changes & mask) {
                bool pressed = (btns & mask) != 0;
                km->feedEdge(b, pressed, now);
            }
        }
        km->tick(now);
    } else {
        // Fallback (no keymap): legacy direct post for rising edges only.
        uint8_t pressed = btns & changes;
        for (uint8_t b = 0; b < 6; b++) {
            uint8_t mask = 1 << b;
            if (pressed & mask) {
                Key k = bitToKey(mask);
                if (k != Key::None) input_->post(k);
            }
        }
    }
}

uint8_t TCA9534Buttons::readRaw() {
    Wire.beginTransmission(I2C_ADDR_TCA9534);
    Wire.write(TCA9534_REG_INPUT);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)I2C_ADDR_TCA9534, (uint8_t)1);
    return (~Wire.read()) & BTN_ALL;  // invert active-LOW, mask 6 buttons
}

Key TCA9534Buttons::bitToKey(uint8_t bit) {
    switch (bit) {
        case BTN_LEFT:   return Key::Left;
        case BTN_DOWN:   return Key::Down;
        case BTN_UP:     return Key::Up;
        case BTN_RIGHT:  return Key::Right;
        case BTN_SELECT: return Key::Select;
        case BTN_CANCEL: return Key::Cancel;
        default:         return Key::None;
    }
}

} // namespace kairo
