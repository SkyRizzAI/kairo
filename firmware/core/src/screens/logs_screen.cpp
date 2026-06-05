#include "kairo/screens/logs_screen.h"
#include "kairo/runtime.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/plugin/plugin_manager.h"
#include "kairo/clock.h"
#include <cstdio>

namespace kairo {

using namespace ui;

LogsScreen::LogsScreen(Runtime& rt) : ComponentScreen(rt, 96) {}

void LogsScreen::enter() {
    scroll_.scrollMain = 0;
    rt_.view().requestRedraw();
}

UiNode* LogsScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();
    char buf[64];

    uint64_t ms = rt.clock().millis();
    uint32_t s = (uint32_t)(ms / 1000) % 60, m = (uint32_t)(ms / 60000) % 60,
             h = (uint32_t)(ms / 3600000);
    if (h > 0) std::snprintf(buf, sizeof(buf), "Uptime: %uh %um %us",
                             (unsigned)h, (unsigned)m, (unsigned)s);
    else       std::snprintf(buf, sizeof(buf), "Uptime: %um %us",
                             (unsigned)m, (unsigned)s);
    rows_.push_back(buf);

    std::snprintf(buf, sizeof(buf), "Apps:   %d loaded",
                  (int)rt.plugins().plugins().size());
    rows_.push_back(buf);

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 3; root.gap = 1;
    Style line; line.height = 1; line.background = true;
    Style sv;   sv.dir = FlexDir::Col; sv.gap = 1;

    UiNode* list = ScrollView(a, scroll_, sv, {});
    UiNode* prev = nullptr;
    for (auto& r : rows_) {
        UiNode* t = Text(a, r.c_str(), TextRole::Body);
        if (!t) break;
        if (!prev) list->firstChild = t; else prev->nextSibling = t;
        prev = t;
    }

    return View(a, root, {
        Text(a, "LOGS", TextRole::Title),
        View(a, line, {}),
        list,
    });
}

} // namespace kairo
