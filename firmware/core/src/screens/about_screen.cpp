#include "nema/screens/about_screen.h"
#include "nema/runtime.h"
#include "nema/board.h"
#include "nema/ui/view_dispatcher.h"
#include "nema/ui/widgets.h"
#include "nema/input/input_action.h"
#include "nema/system/system_info.h"
#include "nema/system/capability_registry.h"
#include "nema/clock.h"
#include <cstdio>

namespace nema {

using namespace aether::ui;

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
    state_.focus.focused = 0;
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

    uint64_t ms = rt.clock().millis();
    uint32_t s  = (uint32_t)(ms / 1000) % 60;
    uint32_t m  = (uint32_t)(ms / 60000) % 60;
    uint32_t h  = (uint32_t)(ms / 3600000);
    char uptimeBuf[24];
    if (h > 0)
        std::snprintf(uptimeBuf, sizeof(uptimeBuf), "%uh %um %us", (unsigned)h, (unsigned)m, (unsigned)s);
    else
        std::snprintf(uptimeBuf, sizeof(uptimeBuf), "%um %us", (unsigned)m, (unsigned)s);
    rows_.push_back(uptimeBuf);

    const auto& capList = rt.capabilities().list();
    rows_.reserve(1 + capList.size());
    for (const auto& cap : capList)
        rows_.push_back(cap);

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

    append(ListSection(a, "Firmware"));
    append(infoRow("Board",    info.boardName.empty()    ? "-" : info.boardName.c_str()));
    append(infoRow("Platform", info.platformName.empty() ? "-" : info.platformName.c_str()));
    append(infoRow("Version",  info.firmwareVersion.empty() ? "-" : info.firmwareVersion.c_str()));
    append(infoRow("Uptime",   rows_[0].c_str()));

    if (!capList.empty()) {
        append(ListSection(a, "Capabilities"));
        for (size_t i = 0; i < capList.size(); i++) {
            ListEntry e; e.label = rows_[1 + i].c_str();
            append(ListItemRow(a, e));
        }
    }

    return View(a, root, { TitleBar(a, "About"), list });
}

} // namespace nema
