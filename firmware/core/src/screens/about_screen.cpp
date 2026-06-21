#include "nema/screens/about_screen.h"
#include "nema/runtime.h"
#include "nema/board.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/ascii_board_renderer.h"
#include "nema/ui/widgets.h"
#include "nema/input/input_action.h"
#include "nema/system/system_info.h"
#include "nema/system/capability_registry.h"
#include "nema/clock.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

// ── Plan 70: AboutModal — Dialog demo shown when Activate is pressed on About ─

class AboutModal : public ComponentScreen {
public:
    explicit AboutModal(Runtime& rt) : ComponentScreen(rt, 16) {}

    ScreenMode mode()        const override { return ScreenMode::Modal; }
    uint16_t   modalWidth()  const override { return 210; }
    uint16_t   modalHeight() const override { return 110; }

    void onAction(input::Action a) override {
        if (a == input::Action::Activate || a == input::Action::Back)
            rt_.view().goBack();
    }

    UiNode* build(NodeArena& a, Runtime& rt) override {
        const auto& info = rt.info();
        char body[128];
        std::snprintf(body, sizeof(body), "%s\n%s\nFW: %s",
                      info.boardName.c_str(),
                      info.platformName.c_str(),
                      info.firmwareVersion.c_str());
        DialogButton ok = {"OK", nullptr, nullptr};
        return Dialog(a, "ABOUT PALANU", body, nullptr, 0, 0, &ok, 1);
    }
};

AboutScreen::AboutScreen(Runtime& rt) : ComponentScreen(rt, 96) {
    aboutModal_ = std::unique_ptr<ComponentScreen>(new AboutModal(rt));
}

void AboutScreen::onResume() {
    scroll_.scrollMain = 0;
    rt_.view().requestRedraw();
}

void AboutScreen::onAction(input::Action a) {
    if (a == input::Action::Activate) {
        if (aboutModal_) rt_.view().navigate(*aboutModal_);
        return;
    }
    ComponentScreen::onAction(a);
}

UiNode* AboutScreen::build(NodeArena& a, Runtime& rt) {
    rows_.clear();
    const auto& info = rt.info();
    char buf[64];
    if (!info.boardName.empty()) {
        std::snprintf(buf, sizeof(buf), "Board: %s", info.boardName.c_str());
        rows_.push_back(buf);
    }
    if (!info.platformName.empty()) {
        std::snprintf(buf, sizeof(buf), "Plat:  %s", info.platformName.c_str());
        rows_.push_back(buf);
    }
    std::snprintf(buf, sizeof(buf), "FW:    %s", info.firmwareVersion.c_str());
    rows_.push_back(buf);

    uint64_t ms = rt.clock().millis();
    uint32_t s = (uint32_t)(ms / 1000) % 60, m = (uint32_t)(ms / 60000) % 60,
             h = (uint32_t)(ms / 3600000);
    if (h > 0) std::snprintf(buf, sizeof(buf), "Up:    %uh %um %us",
                             (unsigned)h, (unsigned)m, (unsigned)s);
    else       std::snprintf(buf, sizeof(buf), "Up:    %um %us",
                             (unsigned)m, (unsigned)s);
    rows_.push_back(buf);

    rows_.push_back("");

    auto ascii = AsciiBoardRenderer::render(rt.board().profile(), 28, 10);
    for (auto& line : ascii) {
        rows_.push_back(line);
    }

    rows_.push_back("");

    auto legend = AsciiBoardRenderer::renderLegend(rt.board().profile());
    for (auto& line : legend) {
        rows_.push_back(line);
    }

    rows_.push_back("");
    rows_.push_back("Capabilities:");
    for (const auto& cap : rt.capabilities().list()) {
        std::snprintf(buf, sizeof(buf), "  %s", cap.c_str());
        rows_.push_back(buf);
    }

    Style root; root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 3; root.gap = 1;
    root.align = Align::Stretch;
    Style sv;   sv.dir = FlexDir::Col; sv.gap = 1;

    // About is read-only — plain Text rows, none focusable. With no focus target,
    // Prev/Next fall through to the scroll-view nudge (dispatchNav), so the screen
    // pans like a document. No focus rings here (Plan 79 smart-scroll only applies
    // where there are selectable items).
    UiNode* list = ScrollView(a, scroll_, sv, {});
    UiNode* prev = nullptr;
    for (auto& r : rows_) {
        UiNode* t = Text(a, r.c_str(), TextRole::Body);
        if (!t) break;
        if (!prev) list->firstChild = t; else prev->nextSibling = t;
        prev = t;
    }

    return View(a, root, {
        TitleBar(a, "ABOUT"),
        list,
    });
}

} // namespace nema
