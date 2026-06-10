#pragma once
#include <cstdint>
#include "kairo/system/board_profile.h"
// Kairo Dev Board pin map — verified against refs/oniondao-badge pinout.
// ESP32-S3-WROOM-1-N8R8. Single source of truth for all board pin constants.

namespace kairo::devboard {

// Power gating
constexpr int PIN_PWR    = 18;  // HIGH = peripheral VCC rail on
constexpr int PIN_SE_EN  = 8;   // HIGH = ATECC608B secure element enabled

// I²C bus (TCA9534 buttons + ATECC608B)
constexpr int PIN_SCL     = 9;
constexpr int PIN_SDA     = 10;
constexpr int PIN_BTN_IRQ = 1;  // TCA9534 INT, active LOW, falling edge on press

// E-ink display — 2.7" 264×176, GxEPD2_270_GDEY027T91
constexpr int PIN_EPD_SCK  = 11;  // SPI SCK
constexpr int PIN_EPD_MOSI = 17;  // SPI MOSI (MISO unused = -1)
constexpr int PIN_EPD_CS   = 12;  // active LOW
constexpr int PIN_EPD_DC   = 13;  // LOW=command, HIGH=data
constexpr int PIN_EPD_RST  = 14;  // active LOW reset
constexpr int PIN_EPD_BUSY = 21;  // HIGH while panel is refreshing

// I²C peripheral addresses
constexpr int I2C_ADDR_TCA9534  = 0x20;
constexpr int I2C_ADDR_ATECC608 = 0x60;

// TCA9534 registers
constexpr uint8_t TCA9534_REG_INPUT  = 0x00;
constexpr uint8_t TCA9534_REG_CONFIG = 0x03;

// Button bit masks (active-LOW on expander; invert when reading)
// Maps directly to Key enum: Left=bit0, Down=1, Up=2, Right=3, Select=4, Cancel=5
constexpr uint8_t BTN_LEFT   = 1 << 0;
constexpr uint8_t BTN_DOWN   = 1 << 1;
constexpr uint8_t BTN_UP     = 1 << 2;
constexpr uint8_t BTN_RIGHT  = 1 << 3;
constexpr uint8_t BTN_SELECT = 1 << 4;
constexpr uint8_t BTN_CANCEL = 1 << 5;
constexpr uint8_t BTN_ALL    = 0x3F;

// ── Board Profile (physical layout) ───────────────────────────────────────
// Dev Board: e-ink LCD top, D-pad cross below left, Select+Cancel below right.
//
// ┌────────────────────────────┐
// │            LCD             │
// └────────────────────────────┘
//      ①                  ⑤
//   ②    ③               ⑥
//      ④
// Up=1 Left=2 Right=3 Down=4 Select=5 Cancel=6

constexpr ComponentDef kDevComponents[] = {
    // id  label      type               x      y      w      h      remote key
    { 1, "Up",      ComponentType::Button,  0.13f, 0.72f, 0.07f, 0.07f, Key::Up     },
    { 2, "Left",    ComponentType::Button,  0.04f, 0.81f, 0.07f, 0.07f, Key::Left   },
    { 3, "Right",   ComponentType::Button,  0.22f, 0.81f, 0.07f, 0.07f, Key::Right  },
    { 4, "Down",    ComponentType::Button,  0.13f, 0.90f, 0.07f, 0.07f, Key::Down   },
    { 5, "Select",  ComponentType::Button,  0.70f, 0.78f, 0.10f, 0.07f, Key::Select },
    { 6, "Cancel",  ComponentType::Button,  0.83f, 0.90f, 0.10f, 0.07f, Key::Cancel },
    { 7, "LCD",     ComponentType::Display, 0.04f, 0.04f, 0.92f, 0.66f },
};

constexpr BoardProfile kDevProfile = {
    "dev-board", "Kairo Dev Board",
    90.0f, 55.0f,
    kDevComponents, 7
};

} // namespace kairo::devboard
