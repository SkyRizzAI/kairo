#include "nema/screens/battery_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/service/service_container.h"
#include "nema/hal/battery.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

BatterySettingsScreen::BatterySettingsScreen(Runtime& rt) : ComponentScreen(rt, 64) {}

void BatterySettingsScreen::onResume() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    lastRedraw_ = 0;
    rt_.view().requestRedraw();
}

void BatterySettingsScreen::tick(uint64_t nowMs) {
    if (nowMs - lastRedraw_ < 1000) return;   // battery changes slowly
    lastRedraw_ = nowMs;
    rt_.view().requestRedraw();
}

UiNode* BatterySettingsScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();
    auto* bat = rt.container().resolve<IBatteryDriver>();

    MenuBuilder m(a, scroll_, this);
    m.section("Battery");
    if (!bat) {
        m.info("No battery driver", nullptr);
        return m.build();
    }
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d%%", bat->level());
    rows_.push_back(buf);
    m.info("Level", rows_[0].c_str());
    m.info("Charging", bat->isCharging() ? "Yes" : "No");
    return m.build();
}

} // namespace nema
