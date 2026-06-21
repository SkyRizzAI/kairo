#include "nema/screens/controls_screen.h"
#include "nema/runtime.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/services/input_service.h"
#include "nema/input/i_key_map.h"
#include "nema/input/input_action.h"
#include "nema/service/service_container.h"
#include "nema/config/config_store.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

ControlsScreen::ControlsScreen(Runtime& rt) : ComponentScreen(rt, 96) {}

void ControlsScreen::onResume() {
    scroll_.scrollMain = 0;
    state_.focus.focused = 0;
    rt_.view().requestRedraw();
}

UiNode* ControlsScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();
    auto& input = rt.input();
    auto* km    = input.keyMap();

    static const input::Action kActions[] = {
        input::Action::Prev, input::Action::Next, input::Action::Activate,
        input::Action::Back, input::Action::AdjustUp, input::Action::AdjustDown,
        input::Action::Menu,
    };

    uint32_t longMs = 500, chordMs = 80;
    if (auto* cfg = rt.container().resolve<IConfigStore>()) {
        longMs  = (uint32_t)cfg->getIntOr("input", "long_ms",  (int64_t)longMs);
        chordMs = (uint32_t)cfg->getIntOr("input", "chord_ms", (int64_t)chordMs);
    }

    // Pre-format all value strings before building UI nodes.
    // Board row
    rows_.push_back(km ? km->boardName() : "simulator");   // [0] board name
    char btnBuf[8]; std::snprintf(btnBuf, sizeof(btnBuf), "%d", km ? km->buttonCount() : 0);
    rows_.push_back(btnBuf);                                // [1] button count

    // Action hint rows
    size_t actionBase = rows_.size();
    for (input::Action act : kActions) {
        const char* hint = input.hintFor(act);
        bool reachable   = input.canReach(act);
        std::string val  = (hint[0] ? hint : "-");
        if (!reachable) val += " (N/A)";
        rows_.push_back(val);
    }

    // Gesture rows
    char longBuf[16], chordBuf[16];
    std::snprintf(longBuf,  sizeof(longBuf),  ">= %ums", (unsigned)longMs);
    std::snprintf(chordBuf, sizeof(chordBuf), "<= %ums", (unsigned)chordMs);
    rows_.push_back(longBuf);    // actionBase + 7
    rows_.push_back(chordBuf);   // actionBase + 8

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.align = Align::Stretch;

    auto infoRow = [&](const char* label, const char* value) {
        ListEntry e; e.label = label; e.value = value;
        return ListItemRow(a, e);
    };

    UiNode* list = ListContainer(a, scroll_, {});
    UiNode* prev = nullptr;
    auto append = [&](UiNode* n) {
        if (!n) return;
        if (!prev) list->firstChild = n; else prev->nextSibling = n;
        prev = n;
    };

    append(ListSection(a, "Board"));
    append(infoRow("Board",   rows_[0].c_str()));
    if (km) append(infoRow("Buttons", rows_[1].c_str()));

    append(ListSection(a, "Actions"));
    for (size_t i = 0; i < sizeof(kActions)/sizeof(kActions[0]); i++)
        append(infoRow(input::actionName(kActions[i]), rows_[actionBase + i].c_str()));

    append(ListSection(a, "Gestures"));
    append(infoRow("Long Press",  rows_[actionBase + 7].c_str()));
    append(infoRow("Chord",       rows_[actionBase + 8].c_str()));

    return View(a, root, { TitleBar(a, "Controls"), list });
}

} // namespace nema
