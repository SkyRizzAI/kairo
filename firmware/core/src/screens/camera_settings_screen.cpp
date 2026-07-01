#include "nema/screens/camera_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/services/camera_service.h"
#include "nema/hal/camera.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

#define S(u) static_cast<CameraSettingsScreen*>(u)

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

        // Acquire-on-use capture test: open → grab one frame → close (the driver
        // owns the frame buffer, so no big allocation here). Camera stays lazy.
        m.section("Test");
        m.nav("Capture test", [](void* u){
            auto* s = S(u);
            auto* cam = s->rt_.camera().get(0);
            if (!cam) { s->testResult_ = "no camera"; s->rt_.view().requestRedraw(); return; }
            bool opened = cam->isOpen() || cam->open();
            if (!opened) { s->testResult_ = "open failed"; s->rt_.view().requestRedraw(); return; }
            const uint8_t* fr = cam->captureFrame();
            char buf[24];
            if (fr) std::snprintf(buf, sizeof(buf), "OK %dx%d",
                                  (int)cam->frameWidth(), (int)cam->frameHeight());
            else    std::snprintf(buf, sizeof(buf), "no frame");
            cam->close();
            s->testResult_ = buf;
            s->rt_.view().requestRedraw();
        });
        if (!testResult_.empty())
            m.info("Result", testResult_.c_str());
    }

    return m.build();
}

#undef S

} // namespace nema
