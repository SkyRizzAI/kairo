#include "nema/screens/camera_settings_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/services/camera_service.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

CameraSettingsScreen::CameraSettingsScreen(Runtime& rt) : ComponentScreen(rt, 64) {}

void CameraSettingsScreen::onResume() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    rt_.view().requestRedraw();
}

UiNode* CameraSettingsScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    UiNode* list = ListContainer(a, scroll_, {});
    UiNode* prev = nullptr;
    auto append = [&](UiNode* n) {
        if (!n) return;
        if (!prev) list->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };

    append(ListSection(a, "Cameras"));

    if (rt.camera().count() == 0) {
        ListEntry e; e.label = "No camera hardware";
        append(ListItemRow(a, e));
    } else {
        rows_.reserve((size_t)rt.camera().count());
        for (int i = 0; i < rt.camera().count(); i++) {
            auto* cam = rt.camera().get(i);
            char buf[24];
            std::snprintf(buf, sizeof(buf), "%dx%d",
                          (int)cam->frameWidth(), (int)cam->frameHeight());
            rows_.push_back(buf);
        }
        for (int i = 0; i < rt.camera().count(); i++) {
            ListEntry e;
            e.label = rt.camera().desc(i);
            e.value = rows_[(size_t)i].c_str();
            append(ListItemRow(a, e));
        }
    }

    return View(a, root, { list });
}

} // namespace nema
