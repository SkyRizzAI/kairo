#include "kairo/screens/close_and_open_modal.h"
#include "kairo/app/app_host_manager.h"
#include "kairo/app/app.h"
#include "kairo/runtime.h"
#include "kairo/ui/canvas.h"
#include "kairo/ui/view_dispatcher.h"
#include <cstdio>

namespace kairo {

CloseAndOpenModal::CloseAndOpenModal(Runtime& rt, AppHostManager& mgr, IApp& target)
    : rt_(rt), mgr_(mgr), target_(target) {}

void CloseAndOpenModal::enter() { cursor_ = 0; rt_.view().requestRedraw(); }

void CloseAndOpenModal::onAction(input::Action a) {
    using A = input::Action;
    switch (a) {
        case A::Prev: case A::Next:
        case A::AdjustUp: case A::AdjustDown:
            cursor_ ^= 1; rt_.view().requestRedraw(); break;
        case A::Activate:
            if (cursor_ == 0) {              // Close & Open
                rt_.view().pop();            // dismiss modal first
                mgr_.killPaused();
                mgr_.doLaunch(target_);
            } else {                          // Cancel
                rt_.view().pop();
            }
            break;
        case A::Back:
            rt_.view().pop();
            break;
        default: break;
    }
}

void CloseAndOpenModal::draw(Canvas& c) {
    // GuiService (Modal mode) already drew the backdrop box; paint text inside it.
    uint16_t mw = modalWidth(), mh = modalHeight();
    uint16_t mx = (c.width()  - mw) / 2;
    uint16_t my = (c.height() - mh) / 2;
    uint16_t x = (uint16_t)(mx + 6);
    uint16_t y = (uint16_t)(my + 6);

    char line[48];
    std::snprintf(line, sizeof(line), "%s is running.", mgr_.pausedName() ? mgr_.pausedName() : "App");
    c.drawText(x, y, line, true); y += 11;
    std::snprintf(line, sizeof(line), "Close to open %s?", target_.name());
    c.drawText(x, y, line, true); y += 16;

    // Two options; highlight the selected one with an inverted bar.
    const char* opts[2] = {"[ Close & Open ]", "[ Cancel ]"};
    for (int i = 0; i < 2; i++) {
        bool sel = (cursor_ == i);
        uint16_t oy = (uint16_t)(y + i * 12);
        if (sel) c.invertRect((uint16_t)(x - 2), (uint16_t)(oy - 1),
                              (uint16_t)(c.textWidth(opts[i]) + 4), 11);
        c.drawText(x, oy, opts[i], !sel);
    }
}

} // namespace kairo
