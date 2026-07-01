#include "nema/screens/sensors_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/services/sensor_service.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

SensorsSettingsScreen::SensorsSettingsScreen(Runtime& rt) : ComponentScreen(rt, 128) {}

void SensorsSettingsScreen::onResume() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    lastRead_ = 0;                 // force an immediate sample on the next tick
    rt_.view().requestRedraw();
}

void SensorsSettingsScreen::tick(uint64_t nowMs) {
    // Sample over I²C at ~2 Hz (not every frame), then redraw with fresh values.
    if (nowMs - lastRead_ < 500) return;
    lastRead_ = nowMs;
    auto& s = rt_.sensors();
    for (int i = 0; i < s.count(); i++)
        if (s.sensor(i)) s.sensor(i)->read();
    rt_.view().requestRedraw();
}

UiNode* SensorsSettingsScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();
    auto& sensors = rt.sensors();

    MenuBuilder m(a, scroll_, this);
    if (sensors.count() == 0) {
        m.section("Sensors");
        m.info("No sensors", nullptr);
        return m.build();
    }

    // Reserve enough for every channel up front so push_back never reallocates
    // (keeps earlier .c_str() pointers valid for the info rows already added).
    size_t total = 0;
    for (int i = 0; i < sensors.count(); i++)
        total += (size_t)(sensors.sensor(i) ? sensors.sensor(i)->channelCount() : 0);
    rows_.reserve(total);

    for (int i = 0; i < sensors.count(); i++) {
        ISensor* s = sensors.sensor(i);
        if (!s) continue;
        m.section(s->label());
        for (int c = 0; c < s->channelCount(); c++) {
            char buf[24];
            std::snprintf(buf, sizeof(buf), "%.2f %s", (double)s->value(c), s->channelUnit(c));
            rows_.push_back(buf);
            m.info(s->channelName(c), rows_.back().c_str());
        }
    }

    return m.build();
}

} // namespace nema
