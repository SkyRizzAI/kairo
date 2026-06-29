#pragma once
#include "nema/board.h"
#include "nema/skyrizzsolana/board_config.h"
#include "nema/skyrizzsolana/tca9534.h"
#include "nema/skyrizzsolana/lcd_driver.h"
#include "nema/skyrizzsolana/solana_key_map.h"
#include "nema/skyrizzsolana/touch_panel.h"
#include "nema/skyrizzsolana/se050_driver.h"

namespace nema::skyrizzsolana {

// SkyRizzSolana — board layer for the SkyRizz Solana ("Lanyard v2") device.
// ESP32-S3-WROOM-1-N16R8 + ILI9341 TFT (direct SPI) + TCA9534 6-button D-pad +
// auto-detecting FT6336U/TSC2007 touch + SE050C2 secure element + WS2812 RGB.
class SkyRizzSolana : public IBoard {
public:
    const char* name() const override { return "skyrizz-solana"; }
    void describeHardware(Runtime& rt) override;
    const BoardProfile& profile() const override { return kSolanaProfile; }

private:
    Tca9534      expander_;
    LcdDriver    lcd_;
    SolanaKeyMap keyMap_;
    TouchPanel   touch_;
    Se050Driver  secure_;
};

} // namespace nema::skyrizzsolana
