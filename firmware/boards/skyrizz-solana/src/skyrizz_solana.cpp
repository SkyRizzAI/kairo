#include "nema/system/capabilities.h"
#include "nema/skyrizzsolana/skyrizz_solana.h"
#include "nema/skyrizzsolana/board_config.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/system/hardware_registry.h"
#include "nema/system/capability_registry.h"
#include "nema/service/service_container.h"
#include "nema/hal/display.h"
#include "nema/config/config_store.h"
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include <Wire.h>
#include <Arduino.h>
#include <cstring>
#include <cstdlib>

// Bring-up switch: when 1, only the essentials are brought up (I²C + buttons +
// LCD). Touch, LED, secure element and the battery ADC are skipped, to isolate a
// boot hang / blank screen on the SkyRizz Solana to one of those drivers.
//
// TEMPORARY: default is 1 (MINIMAL) so CI/GitHub Actions ships a minimal solana
// build for the supplier to test on real hardware. Flip back to 0 (full board)
// once the board is confirmed booting.
#ifndef SOLANA_MINIMAL_BRINGUP
#define SOLANA_MINIMAL_BRINGUP 1
#endif

namespace nema::skyrizzsolana {

void SkyRizzSolana::describeHardware(Runtime& rt) {
    // Shared I²C bus — used by TCA9534, FT6336U/TSC2007 touch, SE050.
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(I2C_FREQ_HZ);

    // Input keymap — must be installed before expander.start() posts events.
    rt.input().setKeyMap(&keyMap_);
    if (!rt.input().validateFloor())
        rt.log().error("SkyRizzSolana", "input floor validation failed!");

    // TCA9534 expander (6 push buttons only)
    expander_.init(rt);
    expander_.setKeyMap(&keyMap_);
    rt.container().registerService(&expander_);
    rt.hardware().add({"expander", DriverKind::Other, "TCA9534 8-bit I2C @0x20"});

    // ILI9341 at ~2.8" is ~143 DPI, below the 200-DPI auto-detect threshold for 2×.
    // Seed 2× as the first-boot default; user can override via Settings.
    if (auto* cfg = rt.container().resolve<IConfigStore>())
        if (cfg->getIntOr("aether", "scale", 0) == 0)
            cfg->setInt("aether", "scale", 200);

    // First-boot default display rotation. The Solana's natural orientation is 270°
    // relative to the E32 (large display bezel on the left). Seed once; the user can
    // change it in Settings → Display. Must run before keyMap_/lcd_ read it below.
    if (auto* cfg = rt.container().resolve<IConfigStore>())
        if (cfg->getIntOr("display", "rotation", -1) < 0)
            cfg->setInt("display", "rotation", 3);

    // Gesture timing from config (or defaults).
    uint32_t longMs = 500;
    if (auto* cfg = rt.container().resolve<IConfigStore>())
        longMs = (uint32_t)cfg->getIntOr("input", "long_ms", (int64_t)longMs);
    keyMap_.setLongMs(longMs);

    // Display rotation → remap directional buttons per orientation (Plan 92 Fase A).
    if (auto* cfg = rt.container().resolve<IConfigStore>())
        keyMap_.setRotation((uint8_t)(cfg->getIntOr("display", "rotation", 0) & 3));
    rt.events().subscribe(events::DisplayRotationChanged, [this](const Event& e) {
        for (const auto& f : e.payload)
            if (std::strcmp(f.key, "rotation") == 0)
                keyMap_.setRotation((uint8_t)(std::atoi(f.value.c_str()) & 3));
    });

    // LCD display (ILI9341 240×320, direct SPI + direct-GPIO backlight/reset)
    lcd_.init(rt);
    rt.container().registerService(&lcd_);
    rt.container().registerAs<IDisplayDriver>(&lcd_);
    rt.hardware().add({"display", DriverKind::Display, "ILI9341 240x320 TFT (SPI)"});
    rt.capabilities().add(caps::Display);

    // Button capabilities — D-pad gives 4 arrows → full 2D directional input.
    rt.hardware().add({"buttons", DriverKind::Other, "TCA9534 6-button D-pad+OK+Back"});
    rt.capabilities().add(caps::Input);
    rt.capabilities().add(caps::InputPrev);
    rt.capabilities().add(caps::InputNext);
    rt.capabilities().add(caps::InputActivate);
    rt.capabilities().add(caps::InputBack);
    rt.capabilities().add(caps::InputAdjust);
    rt.capabilities().add(caps::Input2D);

#if !SOLANA_MINIMAL_BRINGUP
    // Touch — auto-detect FT6336U (cap) or TSC2007 (resistive). Only claim the
    // capability if a controller actually ACKed (honest reporting).
    touch_.init(rt);
    touch_.attachInput(&rt.input());
    rt.container().registerService(&touch_);
    // start() runs on registerServices(); but capability must be declared here.
    // We probe lazily in start(), so optimistically register the service + cap and
    // let the driver no-op if nothing answered (matches "probe both" intent).
    rt.hardware().add({"touch", DriverKind::Other, "FT6336U @0x38 / TSC2007 @0x4A (auto)"});
    rt.capabilities().add(caps::InputTouch);

    // RGB LEDs — WS2812 ×2 on GPIO2, driven via the LED HAL (rt.led()).
    rgb_.begin();
    rt.led().addLed(&rgb_, "rgb0", "WS2812 x2 (GPIO2)");
    rt.hardware().add({"rgb", DriverKind::Other, "WS2812 x2 GPIO2"});
    rt.capabilities().add(caps::Rgb);
    rt.capabilities().add(caps::Led);
    rt.capabilities().add(caps::LedRgb);

    // Secure element — NXP SE050C2 (@0x48, enable via GPIO8). Only claim the
    // capability if the chip ACKed and the wrap/unwrap self-test passed; otherwise
    // the wallet auto-falls-back to software (Se050Driver::hasFeature gate).
    secure_.init(rt);
    rt.container().registerAs<ISecureElement>(&secure_);
    rt.hardware().add({"secure", DriverKind::Other, "NXP SE050C2 @0x48"});
    if (secure_.present()) rt.capabilities().add(caps::Secure);

    // Battery — real ADC gauge on GPIO1 (R18/R19 divider). Registered before the
    // runtime's dummy-battery fallback so this one wins (see Runtime::registerServices).
    battery_.init(rt);
    rt.container().registerService(&battery_);
    rt.container().registerAs<IBatteryDriver>(&battery_);
    rt.hardware().add({"battery", DriverKind::Battery, "ADC gauge (GPIO1 divider)"});
    rt.capabilities().add(caps::Battery);
#else
    rt.log().warn("SkyRizzSolana", "MINIMAL bring-up: touch/LED/secure/battery DISABLED");
#endif  // !SOLANA_MINIMAL_BRINGUP

    rt.capabilities().add(nema::caps::UiExtended);  // 8MB PSRAM → 512 nodes OK
    rt.capabilities().add(nema::caps::UiMomentum);  // 240 MHz → flick scroll OK

    rt.log().info("SkyRizzSolana", "hardware described",
        {{"mcu", "ESP32-S3-WROOM-1-N16R8"}, {"flash", "16MB"}, {"psram", "8MB"}});
}

} // namespace nema::skyrizzsolana
