#pragma once
#include "nema/board.h"
#include "nema/hal/async_display.h"
#include "nema/devboard/board_config.h"
#include "nema/devboard/tca9534_buttons.h"
#include "nema/devboard/dev_board_key_map.h"
#include "nema/devboard/eink_display.h"
namespace nema {

// Palanu Dev Board — ESP32-S3-WROOM-1 + e-ink 264×176 + TCA9534 6-button.
// Hardware testing tier (interim before Palanu Board V1).
class DevBoard : public IBoard {
public:
    const char* name() const override { return "dev-board"; }
    void describeHardware(Runtime& rt) override;
    const BoardProfile& profile() const override { return devboard::kDevProfile; }

    TCA9534Buttons&     buttons() { return buttons_; }
    AsyncDisplayDriver& display() { return display_; }

private:
    DevBoardKeyMap     keyMap_;   // translates TCA9534 edges → Code + Action
    TCA9534Buttons     buttons_;
    EinkDisplay        panel_;
    AsyncDisplayDriver display_;
};

} // namespace nema
