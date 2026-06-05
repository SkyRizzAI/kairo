#pragma once
#include <cstdint>
#include "kairo/system/board_profile.h"

// SkyRizz E32 pin map — single source of truth.
// Reference: dev-board-1-pin_map.md + dev-board-1-pin_cap.md

namespace kairo::skyrizze32 {

// ── I²C bus (shared: XL9535, AHT20, LTR-303ALS, SC7A20, TSC2007, SE050) ──
constexpr int PIN_SCL     = 48;
constexpr int PIN_SDA     = 47;
constexpr int PIN_BUS_INT = 43;   // XL9535 INT# (active-LOW), also UART0 TXD

// ── LCD SPI (TFT, direct ESP32) ───────────────────────────────────────────
constexpr int PIN_LCD_SCLK = 12;
constexpr int PIN_LCD_DC   = 13;
constexpr int PIN_LCD_CS   = 14;   // active-LOW
constexpr int PIN_LCD_MOSI = 21;
// MISO: not connected (write-only LCD)
// Backlight: via XL9535 P00, not a direct GPIO

// ── Touch (TSC2007 resistive) ─────────────────────────────────────────────
constexpr int PIN_TS_INT  = 2;    // PENIRQ, active-LOW

// ── RGB LED chain (WS2812) ────────────────────────────────────────────────
constexpr int PIN_RGB     = 46;   // strapping pin — treat carefully at boot

// ── XL9535 I²C address (A0=A1=A2=GND → 0x20) ────────────────────────────
constexpr int I2C_ADDR_XL9535 = 0x20;
constexpr int I2C_FREQ_HZ     = 400000;   // 400 kHz

// XL9535 registers
constexpr uint8_t XL9535_REG_INPUT0  = 0x00;   // Port 0 input  (P00-P07)
constexpr uint8_t XL9535_REG_INPUT1  = 0x01;   // Port 1 input  (P10-P17)
constexpr uint8_t XL9535_REG_OUTPUT0 = 0x02;
constexpr uint8_t XL9535_REG_OUTPUT1 = 0x03;
constexpr uint8_t XL9535_REG_CONFIG0 = 0x06;   // 1=input  0=output
constexpr uint8_t XL9535_REG_CONFIG1 = 0x07;

// ── XL9535 Port 0 bit assignments ─────────────────────────────────────────
constexpr uint8_t P0_LCD_BLK = 1 << 0;   // P00 — backlight (output, HIGH=on)
constexpr uint8_t P0_TS_RST  = 1 << 1;   // P01 — touch reset (output, HIGH=deasserted)
constexpr uint8_t P0_CAM_RST = 1 << 2;   // P02 — camera reset (output)
constexpr uint8_t P0_SE_RST  = 1 << 3;   // P03 — SE050 reset (output)
constexpr uint8_t P0_SW2     = 1 << 4;   // P04 — SW2 (input, active-LOW)
constexpr uint8_t P0_PB1     = 1 << 5;   // P05 — PB1 (input, active-LOW)
constexpr uint8_t P0_PB2     = 1 << 6;   // P06 — PB2 (input, active-LOW)
constexpr uint8_t P0_EXT_P1  = 1 << 7;   // P07 — external P1 (IO 2)

// ── XL9535 Port 1 bit assignments ─────────────────────────────────────────
constexpr uint8_t P1_EXT_P6  = 1 << 0;   // P10 — external P6 (IO 3)
constexpr uint8_t P1_SW3     = 1 << 1;   // P11 — SW3 + external P7 (shared)
constexpr uint8_t P1_SW1     = 1 << 2;   // P12 — SW1 (input, active-LOW)
constexpr uint8_t P1_EXT_P5  = 1 << 3;   // P13 — external P5
constexpr uint8_t P1_EXT_P4  = 1 << 4;   // P14 — external P4
constexpr uint8_t P1_EXT_P3  = 1 << 5;   // P15 — external P3
constexpr uint8_t P1_EXT_P2  = 1 << 6;   // P16 — external P2
constexpr uint8_t P1_IND_LED = 1 << 7;   // P17 — indicator LED (output, HIGH=on)

// ── Button IDs for IKeyMap (0-indexed) ────────────────────────────────────
// 3 buttons BELOW the LCD:  Left | OK/Back | Right
constexpr uint8_t BTN_LEFT   = 0;   // SW1 → XL9535 P12 (Port 1, bit 2) — Left arrow
constexpr uint8_t BTN_MIDDLE = 1;   // SW2 → XL9535 P04 (Port 0, bit 4) — OK / hold = Back
constexpr uint8_t BTN_RIGHT  = 2;   // SW3 → XL9535 P11 (Port 1, bit 1) — Right arrow
// 2 buttons on the RIGHT SIDE of the LCD:  Up (top) | Down (bottom)
// Physically PB2 (P06) is the top button and PB1 (P05) the bottom — wired in
// xl9535.cpp feedBtn(); swap there if the panel revision differs.
constexpr uint8_t BTN_UP     = 3;   // top    side button → Up arrow
constexpr uint8_t BTN_DOWN   = 4;   // bottom side button → Down arrow


// ── I2S Audio (ES7243E ADC, FPC2) ─────────────────────────────────────────
constexpr int PIN_I2S_MCLK = 3;    // Master clock out → ES7243E
constexpr int PIN_I2S_BCLK = 0;    // Bit clock (shared: ES7243E + NS4168)
constexpr int PIN_I2S_WS   = 38;   // Word select / LRCK (shared)
constexpr int PIN_I2S_DIN  = 39;   // Data in  (ES7243E SDO → ESP32)
constexpr int PIN_I2S_DOUT = 45;   // Data out (ESP32 → NS4168 SDI)

// ── DVP Camera (GC2145, FPC3) ─────────────────────────────────────────────
constexpr int PIN_CAM_XCLK  = 7;   // Master clock out → GC2145 (20 MHz)
constexpr int PIN_CAM_PCLK  = 17;  // Pixel clock in ← GC2145
constexpr int PIN_CAM_VSYNC = 4;   // VSYNC in
constexpr int PIN_CAM_HREF  = 5;   // HREF / DE in
constexpr int PIN_CAM_D0    = 8;
constexpr int PIN_CAM_D1    = 10;
constexpr int PIN_CAM_D2    = 11;
constexpr int PIN_CAM_D3    = 9;
constexpr int PIN_CAM_D4    = 18;
constexpr int PIN_CAM_D5    = 16;
constexpr int PIN_CAM_D6    = 15;
constexpr int PIN_CAM_D7    = 6;
// Camera reset: P0_CAM_RST (XL9535 P02) — already defined above.

// ── I2C device addresses (media) ──────────────────────────────────────────
constexpr uint8_t I2C_ADDR_ES7243E = 0x11;   // Audio ADC
constexpr uint8_t I2C_ADDR_GC2145  = 0x3C;   // Camera SCCB

// ── Board Profile (physical layout) ───────────────────────────────────────
// SkyRizz E32: TFT LCD center, 3 buttons below, 2 buttons on right side.
//
// ┌────────────────────────┐
// │                        │
// │                        │ 4
// │         LCD            │
// │                        │ 5
// │                        │
// └────────────────────────┘
//  1          2          3

constexpr ComponentDef kE32Components[] = {
    // id  label      type               x      y      w      h
    { 1, "Left",    ComponentType::Button,  0.10f, 0.82f, 0.18f, 0.12f },
    { 2, "OK",      ComponentType::Button,  0.41f, 0.82f, 0.18f, 0.12f },
    { 3, "Right",   ComponentType::Button,  0.72f, 0.82f, 0.18f, 0.12f },
    { 4, "Up",      ComponentType::Button,  0.90f, 0.22f, 0.08f, 0.14f },
    { 5, "Down",    ComponentType::Button,  0.90f, 0.52f, 0.08f, 0.14f },
    { 6, "LCD",     ComponentType::Display, 0.04f, 0.04f, 0.82f, 0.72f },
};

constexpr BoardProfile kE32Profile = {
    "skyrizz-e32", "SkyRizz E32",
    80.0f, 55.0f,
    kE32Components, 6
};

} // namespace kairo::skyrizze32
