#include "nema/screens/camera_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/services/camera_service.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

CameraSettingsScreen::CameraSettingsScreen(Runtime& rt) : ComponentScreen(rt, 64) {}

void CameraSettingsScreen::onResume() {
    rt_.view().requestRedraw();
}

UiNode* CameraSettingsScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();

    MenuBuilder m(a, scroll_, this);
    m.section("Cameras");

    if (rt.camera().count() == 0) {
        ListEntry e; e.label = "No camera hardware";
        m.add(ListItemRow(a, e));
    } else {
        rows_.reserve((size_t)rt.camera().count());
        for (int i = 0; i < rt.camera().count(); i++) {
            auto* cam = rt.camera().get(i);
            char buf[24];
            std::snprintf(buf, sizeof(buf), "%dx%d",
                          (int)cam->frameWidth(), (int)cam->frameHeight());
            rows_.push_back(buf);
        }
        for (int i = 0; i < rt.camera().count(); i++)
            m.info(rt.camera().desc(i), rows_[(size_t)i].c_str());
    }

    return m.build();
}

} // namespace nema
