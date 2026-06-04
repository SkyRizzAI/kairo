#include "kairo/skyrizze32/skyrizz_e32.h"
#include "kairo/skyrizze32/board_config.h"
#include "kairo/runtime.h"
#include "kairo/log/logger.h"
#include "kairo/system/hardware_registry.h"
#include "kairo/system/capability_registry.h"
#include "kairo/service/service_container.h"
#include "kairo/hal/display.h"
#include "kairo/config/config_store.h"
#include <Wire.h>
#include <Arduino.h>

namespace kairo::skyrizze32 {

void SkyRizzE32::describeHardware(Runtime& rt) {
    // Shared I²C bus — used by XL9535, sensors, TSC2007, SE050
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(I2C_FREQ_HZ);

    // Input keymap — must be installed before expander.start() posts events
    rt.input().setKeyMap(&keyMap_);
    if (!rt.input().validateFloor())
        rt.log().error("SkyRizzE32", "input floor validation failed!");

    // XL9535 expander (buttons + backlight + resets)
    expander_.init(rt);
    expander_.setKeyMap(&keyMap_);
    rt.container().registerService(&expander_);
    rt.hardware().add({"expander", DriverKind::Other, "XL9535 16-bit I2C"});

    // Load gesture timing from config (or use defaults)
    uint32_t longMs = 500;
    if (auto* cfg = rt.container().resolve<IConfigStore>())
        longMs = (uint32_t)cfg->getIntOr("input", "long_ms", (int64_t)longMs);
    keyMap_.setLongMs(longMs);

    // LCD display
    lcd_.init(rt, expander_);
    rt.container().registerService(&lcd_);
    rt.container().registerAs<IDisplayDriver>(&lcd_);
    rt.hardware().add({"display", DriverKind::Display, "TFT LCD (FPC1 SPI)"});
    rt.capabilities().add("display");

    // Button capabilities (3-button, no native 2D)
    rt.hardware().add({"buttons", DriverKind::Other, "XL9535 3-button (SW1/PB1/SW2)"});
    rt.capabilities().add("input");
    rt.capabilities().add("input.prev");
    rt.capabilities().add("input.next");
    rt.capabilities().add("input.activate");
    rt.capabilities().add("input.back");
    rt.capabilities().add("input.adjust");
    // NOTE: "input.2d" intentionally NOT added — no Up/Down/Left/Right natively.

    // RGB LEDs
    rt.hardware().add({"rgb", DriverKind::Other, "WS2812 x2 GPIO46"});
    rt.capabilities().add("rgb");

    // Sensors (init-only; data via events or service in future plans)
    rt.hardware().add({"sensors", DriverKind::Other, "AHT20, LTR-303ALS, SC7A20"});
    rt.capabilities().add("sensors.environment");
    rt.capabilities().add("sensors.light");
    rt.capabilities().add("sensors.motion");

    rt.log().info("SkyRizzE32", "hardware described",
        {{"mcu", "ESP32-S3-WROOM-1-N16R8"}, {"flash", "16MB"}, {"psram", "8MB"}});
}

} // namespace kairo::skyrizze32
