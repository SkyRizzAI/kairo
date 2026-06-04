#pragma once
#include "kairo/board.h"
#include "kairo/skyrizze32/xl9535.h"
#include "kairo/skyrizze32/lcd_driver.h"
#include "kairo/skyrizze32/e32_key_map.h"

namespace kairo::skyrizze32 {

// SkyRizzE32 — board layer for the SkyRizz E32 dev board.
// ESP32-S3-WROOM-1-N16R8 + TFT LCD + XL9535 + 3 buttons + sensors.
class SkyRizzE32 : public IBoard {
public:
    const char* name() const override { return "skyrizz-e32"; }
    void describeHardware(Runtime& rt) override;

private:
    Xl9535      expander_;
    LcdDriver   lcd_;
    E32KeyMap   keyMap_;
};

} // namespace kairo::skyrizze32
