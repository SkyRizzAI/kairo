#pragma once
#include <cstdint>

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
// Physical layout: Left=SW1, Middle=PB1, Right=SW2
// Confirmed at bring-up — swap constants if physical order differs.
constexpr uint8_t BTN_LEFT   = 0;   // SW1  → XL9535 P12 (Port 1, bit 2)
constexpr uint8_t BTN_MIDDLE = 1;   // PB1  → XL9535 P05 (Port 0, bit 5)
constexpr uint8_t BTN_RIGHT  = 2;   // SW2  → XL9535 P04 (Port 0, bit 4)
// BTN_PB2 = 3 (P06), BTN_SW3 = 4 (P11) — available for future use

} // namespace kairo::skyrizze32
