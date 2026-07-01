#include "nema/screens/led_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/services/led_service.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

LedSettingsScreen::LedSettingsScreen(Runtime& rt) : ComponentScreen(rt, 128) {}

void LedSettingsScreen::onResume() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    rt_.view().requestRedraw();
}

#define S(u) static_cast<LedSettingsScreen*>(u)

UiNode* LedSettingsScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();
    auto& led = rt.led();

    MenuBuilder m(a, scroll_, this);

    m.section("LEDs");
    if (led.count() == 0) {
        m.info("No LEDs", nullptr);
        return m.build();
    }

    // Pre-format all value strings up front (reserve count + 1 for brightness so a
    // later push_back never reallocates and invalidates earlier .c_str() pointers).
    rows_.reserve((size_t)led.count() + 1);
    for (int i = 0; i < led.count(); i++) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d px, %s", led.led(i)->pixelCount(),
                      led.led(i)->colorModel() == LedColorModel::Rgb ? "RGB" : "mono");
        rows_.push_back(buf);
        m.info(led.led(i)->label(), rows_[(size_t)i].c_str());
    }

    m.section("Test (all LEDs)");
    m.nav("Red",   [](void* u){ S(u)->rt_.led().solid(-1, 255, 0, 0); });
    m.nav("Green", [](void* u){ S(u)->rt_.led().solid(-1, 0, 255, 0); });
    m.nav("Blue",  [](void* u){ S(u)->rt_.led().solid(-1, 0, 0, 255); });
    m.nav("White", [](void* u){ S(u)->rt_.led().solid(-1, 255, 255, 255); });
    m.nav("Blink", [](void* u){ S(u)->rt_.led().blink(-1, 255, 255, 255, 200, 200, -1); });
    m.nav("Off",   [](void* u){ S(u)->rt_.led().off(-1); });

    m.section("Notify");
    m.nav("Working", [](void* u){ S(u)->rt_.led().notify(LedService::Notify::Working); });
    m.nav("Success", [](void* u){ S(u)->rt_.led().notify(LedService::Notify::Success); });
    m.nav("Error",   [](void* u){ S(u)->rt_.led().notify(LedService::Notify::Error); });

    m.section("Brightness");
    char bb[8];
    std::snprintf(bb, sizeof(bb), "%d", brightness_);
    rows_.push_back(bb);
    m.input("Level", rows_.back().c_str(), [](void* u, int d){
        auto* s = S(u);
        s->brightness_ += d * 32;
        if (s->brightness_ < 0)   s->brightness_ = 0;
        if (s->brightness_ > 255) s->brightness_ = 255;
        auto& led = s->rt_.led();
        for (int i = 0; i < led.count(); i++) led.led(i)->setBrightness((uint8_t)s->brightness_);
        led.solid(-1, 255, 255, 255);   // reapply white so the change is visible
    });

    return m.build();
}

#undef S

} // namespace nema
