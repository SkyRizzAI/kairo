#include "kairo/apps/counter_app.h"
#include "kairo/ui/widgets.h"
#include <cstdio>

namespace kairo {

using namespace ui;

void CounterApp::onDec(void* u)   { static_cast<CounterApp*>(u)->count_--; }
void CounterApp::onInc(void* u)   { static_cast<CounterApp*>(u)->count_++; }
void CounterApp::onReset(void* u) { static_cast<CounterApp*>(u)->count_ = 0; }

UiNode* CounterApp::build(NodeArena& a, AppContext&) {
    std::snprintf(buf_, sizeof(buf_), "%d", count_);

    Style root;
    root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 4; root.gap = 8;
    root.align = Align::Center; root.justify = Justify::Center;

    Style row;
    row.dir = FlexDir::Row; row.gap = 8; row.align = Align::Center;

    return View(a, root, {
        Text(a, "Counter", TextRole::Caption),
        Text(a, buf_, TextRole::Title),
        Row(a, row, {
            Button(a, "-", onDec, this),
            Button(a, "+", onInc, this),
        }),
        Button(a, "Reset", onReset, this),
    });
}

} // namespace kairo
