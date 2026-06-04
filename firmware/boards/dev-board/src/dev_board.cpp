#include "kairo/devboard/dev_board.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/system/hardware_registry.h"
#include "kairo/system/capability_registry.h"
#include "kairo/service/service_container.h"
#include "kairo/hal/display.h"
#include "kairo/config/config_store.h"
#include "kairo/devboard/board_config.h"
#include <Wire.h>
#include <Arduino.h>

namespace kairo {

using namespace devboard;

void DevBoard::describeHardware(Runtime& rt) {
    // Power rail + I²C init (mirrors ref init_peripherals())
    pinMode(PIN_PWR,   OUTPUT); digitalWrite(PIN_PWR,   HIGH);
    pinMode(PIN_SE_EN, OUTPUT); digitalWrite(PIN_SE_EN, HIGH);
    delay(50);
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(100000);  // 100 kHz — required for ATECC608B

    // Display — raw panel wrapped by AsyncDisplayDriver for non-blocking flush.
    // Register order matters: panel starts (SPI/GxEPD2 ready) before the wrapper's
    // display task, so the task's first flushBuffer() finds the panel initialised.
    panel_.init(rt.log());
    rt.container().registerService(&panel_);            // start() → SPI + GxEPD2

    display_.init(panel_, rt.log());                    // wrap panel + alloc buffers
    rt.container().registerService(&display_);          // start() → display task
    rt.container().registerAs<IDisplayDriver>(&display_); // Canvas binds to wrapper
    rt.hardware().add({"display", DriverKind::Display, "e-ink GDEY027T91 264x176"});
    rt.capabilities().add("display");

    // Config store (NVS — internal flash, no SD required)
    config_.init(rt.log());
    rt.container().registerService(&config_);
    rt.container().registerAs<IConfigStore>(&config_);

    // Input keymap — install before buttons so TCA9534 posts enriched events.
    rt.input().setKeyMap(&keyMap_);
    if (!rt.input().validateFloor())
        rt.log().error("DevBoard", "input floor validation failed!");

    // Buttons
    buttons_.init(rt);
    rt.container().registerService(&buttons_);
    rt.hardware().add({"buttons", DriverKind::Other, "TCA9534 6-button"});
    rt.capabilities().add("input");
    rt.capabilities().add("input.prev");
    rt.capabilities().add("input.next");
    rt.capabilities().add("input.activate");
    rt.capabilities().add("input.back");
    rt.capabilities().add("input.adjust");
    rt.capabilities().add("input.2d");

    rt.log().info("DevBoard", "hardware described",
        {{"mcu", "ESP32-S3-WROOM-1"}, {"flash", "8MB"}, {"psram", "8MB"}});
}

} // namespace kairo
