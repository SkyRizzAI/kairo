#include "nema/system/capabilities.h"
#include "nema/skyrizze32/skyrizz_e32.h"
#include "nema/skyrizze32/board_config.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/system/hardware_registry.h"
#include "nema/system/capability_registry.h"
#include "nema/service/service_container.h"
#include "nema/hal/display.h"
#include "nema/config/config_store.h"
#include <Wire.h>
#include <Arduino.h>

namespace nema::skyrizze32 {

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
    rt.capabilities().add(caps::Display);

    // Button capabilities (3-button, no native 2D)
    rt.hardware().add({"buttons", DriverKind::Other, "XL9535 3-button (SW1/PB1/SW2)"});
    rt.capabilities().add(caps::Input);
    rt.capabilities().add(caps::InputPrev);
    rt.capabilities().add(caps::InputNext);
    rt.capabilities().add(caps::InputActivate);
    rt.capabilities().add(caps::InputBack);
    rt.capabilities().add(caps::InputAdjust);
    // 5 buttons give 4 distinct arrows (Up/Down on side, Left/Right below) →
    // full 2D directional input, so the virtual keyboard uses grid mode.
    rt.capabilities().add(caps::Input2D);

    // Touch (FT6336U capacitive) — pointer HAL (Plan 29)
    touch_.init(rt, expander_);
    touch_.attachInput(&rt.input());
    rt.container().registerService(&touch_);
    rt.hardware().add({"touch", DriverKind::Other, "FT6336U capacitive @0x38"});
    rt.capabilities().add(caps::InputTouch);

    // RGB LEDs
    rt.hardware().add({"rgb", DriverKind::Other, "WS2812 x2 GPIO46"});
    rt.capabilities().add(caps::Rgb);

    // Sensors (init-only; data via events or service in future plans)
    rt.hardware().add({"sensors", DriverKind::Other, "AHT20, LTR-303ALS, SC7A20"});
    rt.capabilities().add(caps::SensorsEnv);
    rt.capabilities().add(caps::SensorsLight);
    rt.capabilities().add(caps::SensorsMotion);

    // Audio input — ES7243E mic ADC
    mic_.init(rt, expander_);
    rt.container().registerService(&mic_);
    rt.audio().addInput(&mic_, "mic0", "I2S Built-in");
    rt.hardware().add({"audio.input", DriverKind::Other, "ES7243E @0x11"});
    rt.capabilities().add(caps::AudioInput);

    // Audio output — NS4168 I2S amplifier (shares I2S0 TX with the mic)
    speaker_.init(rt, mic_);
    rt.container().registerService(&speaker_);
    rt.audio().addOutput(&speaker_, "spk0", "NS4168 I2S Amp");
    rt.hardware().add({"audio.output", DriverKind::Other, "NS4168 I2S Amp"});
    rt.capabilities().add(caps::AudioOutput);

    // Camera — GC2145 DVP
    camera_.init(rt, expander_);
    rt.container().registerService(&camera_);
    rt.camera().add(&camera_, "cam0", "GC2145 2MP DVP");
    rt.hardware().add({"camera", DriverKind::Other, "GC2145 2MP @0x3C"});
    rt.capabilities().add(caps::Camera);

    // Secure element — NXP SE050 (U18, shared I²C @0x48, reset via XL9535 P03).
    // HW root-of-trust for the generic ISecureElement HAL; apps gate on
    // caps::Secure. Crypto ops are scaffolded (ADR 0005) pending NXP middleware.
    secure_.init(rt, expander_);
    rt.container().registerAs<ISecureElement>(&secure_);
    rt.hardware().add({"secure", DriverKind::Other, "NXP SE050 @0x48"});
    rt.capabilities().add(caps::Secure);

    rt.log().info("SkyRizzE32", "hardware described",
        {{"mcu", "ESP32-S3-WROOM-1-N16R8"}, {"flash", "16MB"}, {"psram", "8MB"}});
}

// TF1 microSD on the shared SPI3 bus. The ESP32 platform mounts this at "/sd".
bool SkyRizzE32::sdSpi(SdSpiConfig& out) const {
    out.sclk   = PIN_SD_SCLK;
    out.miso   = PIN_SD_MISO;
    out.mosi   = PIN_SD_MOSI;
    out.cs     = PIN_SD_CS;
    out.hostId = SD_SPI_HOST;
    return true;
}

} // namespace nema::skyrizze32
