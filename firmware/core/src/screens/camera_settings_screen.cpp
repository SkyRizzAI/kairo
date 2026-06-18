#include "nema/screens/camera_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/services/camera_service.h"
#include <cstdio>

namespace nema {

using namespace ui;

CameraSettingsScreen::CameraSettingsScreen(Runtime& rt) : ComponentScreen(rt, 64) {}

void CameraSettingsScreen::onResume() {
    scroll_.scrollMain = 0;
    rt_.view().requestRedraw();
}

UiNode* CameraSettingsScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();
    char buf[48];
    if (rt.camera().count() == 0) {
        rows_.push_back("No camera hardware");
    } else {
        for (int i = 0; i < rt.camera().count(); i++) {
            auto* cam = rt.camera().get(i);
            std::snprintf(buf, sizeof(buf), "%s  %dx%d", rt.camera().desc(i),
                          (int)cam->frameWidth(), (int)cam->frameHeight());
            rows_.push_back(buf);
        }
    }

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 3; root.gap = 1;
    root.align = Align::Stretch;
    Style sv;   sv.dir = FlexDir::Col; sv.gap = 2;

    UiNode* list = ScrollView(a, scroll_, sv, {});
    UiNode* prev = nullptr;
    for (auto& r : rows_) {
        UiNode* t = Text(a, r.c_str(), TextRole::Body);
        if (!t) break;
        if (!prev) list->firstChild = t; else prev->nextSibling = t;
        prev = t;
    }

    return View(a, root, {
        TitleBar(a, "CAMERA"),
        list,
    });
}

} // namespace nema
