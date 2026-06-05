#pragma once
#include "kairo/board.h"
#include "kairo/skyrizze32/board_config.h"
#include "kairo/skyrizze32/xl9535.h"
#include "kairo/skyrizze32/lcd_driver.h"
#include "kairo/skyrizze32/e32_key_map.h"
#include "kairo/skyrizze32/ft6336_touch.h"
#include "kairo/skyrizze32/es7243e_mic.h"
#include "kairo/skyrizze32/i2s_speaker.h"
#include "kairo/skyrizze32/gc2145_camera.h"

namespace kairo::skyrizze32 {

// SkyRizzE32 — board layer for the SkyRizz E32 dev board.
// ESP32-S3-WROOM-1-N16R8 + TFT LCD + XL9535 + 5 buttons + FT6336U touch + sensors.
class SkyRizzE32 : public IBoard {
public:
    const char* name() const override { return "skyrizz-e32"; }
    void describeHardware(Runtime& rt) override;
    const BoardProfile& profile() const override { return kE32Profile; }

private:
    Xl9535      expander_;
    LcdDriver   lcd_;
    E32KeyMap   keyMap_;
    Ft6336Touch touch_;
    Es7243eMic      mic_;
    I2sSpeaker      speaker_;
    Gc2145Camera    camera_;
};

} // namespace kairo::skyrizze32
