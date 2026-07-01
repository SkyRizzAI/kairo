#pragma once
#include <cstdint>
#include "nema/system/board_profile.h"

// SkyRizz Solana ("Lanyard v2") pin map — single source of truth.
// MCU: ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB Octal PSRAM).
// Reference: refs/SkyRizz-Solana-io.html (lanyard_v2.kicad_sch).
//
// Difference vs SkyRizz E32: backlight, touch-reset and SE-enable are DIRECT
// ESP32 GPIOs here (not behind the I/O expander), and the expander is a smaller
// 8-bit TCA9534 used ONLY for the six push buttons.

namespace nema::skyrizzsolana {

// ── Shared I²C bus (TCA9534, FT6336U/TSC2007 touch, SE050) ──────────────────
constexpr int PIN_SCL     = 9;    // SCL
constexpr int PIN_SDA     = 10;   // SDA
constexpr int I2C_FREQ_HZ = 400000;   // 400 kHz

// ── LCD SPI (TFT, direct ESP32) ─────────────────────────────────────────────
constexpr int PIN_LCD_SCLK = 11;   // LCDSCK
constexpr int PIN_LCD_CS   = 12;   // LCDCS  (active-LOW)
constexpr int PIN_LCD_DC   = 13;   // LCDDC
constexpr int PIN_LCD_RST  = 14;   // LCDRST (active-LOW)
constexpr int PIN_LCD_MOSI = 15;   // LCDMOSI
constexpr int PIN_LCD_MISO = 16;   // LCDMISO (write-only panel → unused)
constexpr int PIN_LCD_BL   = 7;    // LCDBL backlight via transistor Q4 (HIGH=on)

// ── Touch (capacitive FT6336U @0x38 OR resistive TSC2007 @0x4A) ─────────────
// Both controllers sit on the bus; only one panel is fitted. TouchPanel probes
// at start() and drives whichever ACKs.
constexpr int     PIN_TS_INT  = 5;    // TSINT  (PENIRQ, active-LOW)
constexpr int     PIN_TS_RST  = 6;    // TSRST  (direct GPIO, active-LOW)
constexpr uint8_t I2C_ADDR_FT6336 = 0x38;
constexpr uint8_t I2C_ADDR_TSC2007 = 0x4A;

// ── TCA9534 button expander (@0x20) ─────────────────────────────────────────
constexpr int     PIN_PB_INT       = 4;    // PBINT (active-LOW on any button change)
constexpr uint8_t I2C_ADDR_TCA9534 = 0x20; // A0=A1=A2=GND → 0x20

// TCA9534 registers
constexpr uint8_t TCA9534_REG_INPUT    = 0x00;   // input port
constexpr uint8_t TCA9534_REG_OUTPUT   = 0x01;   // output port
constexpr uint8_t TCA9534_REG_POLARITY = 0x02;   // polarity inversion
constexpr uint8_t TCA9534_REG_CONFIG   = 0x03;   // 1=input, 0=output

// ── TCA9534 port bit → physical push button (PB1..PB6 on P0..P5) ────────────
constexpr uint8_t P_PB1 = 1 << 0;   // P0
constexpr uint8_t P_PB2 = 1 << 1;   // P1
constexpr uint8_t P_PB3 = 1 << 2;   // P2
constexpr uint8_t P_PB4 = 1 << 3;   // P3
constexpr uint8_t P_PB5 = 1 << 4;   // P4
constexpr uint8_t P_PB6 = 1 << 5;   // P5
constexpr uint8_t PB_MASK = P_PB1 | P_PB2 | P_PB3 | P_PB4 | P_PB5 | P_PB6;

// ── Button IDs for IKeyMap (0-indexed) — D-pad + OK + Back ──────────────────
// Physical PB→function layout (confirmed by hardware):
//   PB1 (P0) = Up · PB2 (P1) = Left · PB3 (P2) = Right · PB4 (P3) = Down ·
//   PB5 (P4) = OK/Select · PB6 (P5) = Cancel/Back.
// The PB_* mask wired to each id lives in tca9534.cpp::feedButtons() — swap there
// if a panel revision moves a switch.
constexpr uint8_t BTN_UP    = 0;   // PB1 (P0) — Prev
constexpr uint8_t BTN_LEFT  = 1;   // PB2 (P1) — AdjustDown
constexpr uint8_t BTN_RIGHT = 2;   // PB3 (P2) — AdjustUp
constexpr uint8_t BTN_DOWN  = 3;   // PB4 (P3) — Next
constexpr uint8_t BTN_OK    = 4;   // PB5 (P4) — Activate (tap) / Menu (hold)
constexpr uint8_t BTN_BACK  = 5;   // PB6 (P5) — Back

// ── Secure element (NXP SE050C2 @0x48, enable via direct GPIO8) ──────────────
constexpr int     PIN_SE_EN     = 8;     // SE_EN (HIGH = enabled)
constexpr uint8_t I2C_ADDR_SE050 = 0x48;

// ── WS2812 addressable RGB LED chain (D3→D4, 2 LEDs) ────────────────────────
constexpr int PIN_RGB   = 2;
constexpr int RGB_COUNT = 2;

// ── Battery monitor (ADC via R18/R19 divider) ───────────────────────────────
constexpr int PIN_ADC_BAT = 1;

// ── PDM microphones (SPH0641 ×2). NOTE: these pins ARE the ESP32-S3 native USB
// D−/D+ lines — PDM mic and USB are mutually exclusive. Not driven in v1.
constexpr int PIN_MIC_CLK = 19;   // MICSCK (also USB D−)
constexpr int PIN_MIC_DAT = 20;   // MICSDI (also USB D+)

// ── Board Profile (physical layout) ─────────────────────────────────────────
// SkyRizz Solana lanyard: TFT LCD up top, D-pad + OK + Back below.
//
// ┌────────────────────────┐
// │                        │
// │          LCD           │
// │                        │
// └────────────────────────┘
//          [ Up ]
//   [Left] [ OK ] [Right]
//         [Down] [Back]

constexpr ComponentDef kSolanaComponents[] = {
    // id  label    type                    x      y      w      h      remote key
    { 1, "LCD",    ComponentType::Display, 0.10f, 0.04f, 0.80f, 0.50f },
    { 2, "Up",     ComponentType::Button,  0.41f, 0.60f, 0.18f, 0.10f, Key::Up     },
    { 3, "Left",   ComponentType::Button,  0.16f, 0.72f, 0.18f, 0.10f, Key::Left   },
    { 4, "OK",     ComponentType::Button,  0.41f, 0.72f, 0.18f, 0.10f, Key::Select },
    { 5, "Right",  ComponentType::Button,  0.66f, 0.72f, 0.18f, 0.10f, Key::Right  },
    { 6, "Down",   ComponentType::Button,  0.41f, 0.84f, 0.18f, 0.10f, Key::Down   },
    { 7, "Back",   ComponentType::Button,  0.66f, 0.84f, 0.18f, 0.10f, Key::Cancel },
};

constexpr BoardProfile kSolanaProfile = {
    "skyrizz-solana", "SkyRizz Solana",
    40.0f, 80.0f,
    kSolanaComponents, 7
};

} // namespace nema::skyrizzsolana
