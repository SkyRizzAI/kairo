#include "kairo/screens/controls_screen.h"
#include "kairo/runtime.h"
#include "kairo/ui/view_dispatcher.h"
#include "kairo/services/input_service.h"
#include "kairo/input/i_key_map.h"
#include "kairo/input/input_action.h"
#include "kairo/service/service_container.h"
#include "kairo/config/config_store.h"
#include <cstdio>

namespace kairo {

using namespace ui;

ControlsScreen::ControlsScreen(Runtime& rt) : ComponentScreen(rt, 96) {}

void ControlsScreen::enter() {
    scroll_.scrollMain = 0;
    rt_.view().requestRedraw();
}

UiNode* ControlsScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();
    auto& input = rt.input();
    auto* km    = input.keyMap();
    char buf[64];

    std::snprintf(buf, sizeof(buf), "Board: %s", km ? km->boardName() : "simulator");
    rows_.push_back(buf);
    if (km) {
        std::snprintf(buf, sizeof(buf), "Buttons: %d", km->buttonCount());
        rows_.push_back(buf);
    }
    rows_.push_back("");
    rows_.push_back("ACTIONS");

    static const input::Action kActions[] = {
        input::Action::Prev, input::Action::Next, input::Action::Activate,
        input::Action::Back, input::Action::AdjustUp, input::Action::AdjustDown,
        input::Action::Menu,
    };
    for (input::Action act : kActions) {
        const char* hint = input.hintFor(act);
        bool reachable   = input.canReach(act);
        std::snprintf(buf, sizeof(buf), "  %-10s %s%s", input::actionName(act),
                      hint[0] ? hint : "-", reachable ? "" : " (N/A)");
        rows_.push_back(buf);
    }

    rows_.push_back("");
    rows_.push_back("GESTURES");
    uint32_t longMs = 500, chordMs = 80;
    if (auto* cfg = rt.container().resolve<IConfigStore>()) {
        longMs  = (uint32_t)cfg->getIntOr("input", "long_ms",  (int64_t)longMs);
        chordMs = (uint32_t)cfg->getIntOr("input", "chord_ms", (int64_t)chordMs);
    }
    std::snprintf(buf, sizeof(buf), "  Long  >= %ums", (unsigned)longMs);  rows_.push_back(buf);
    std::snprintf(buf, sizeof(buf), "  Short <  %ums", (unsigned)longMs);  rows_.push_back(buf);
    std::snprintf(buf, sizeof(buf), "  Chord <= %ums", (unsigned)chordMs); rows_.push_back(buf);

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
        Text(a, "CONTROLS", TextRole::Title),
        View(a, line, {}),
        list,
    });
}

} // namespace kairo
