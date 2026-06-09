#include "kairo/apps/counter_app.h"
#include "kairo/ui/widgets.h"
#include <cstdio>

namespace kairo {

using namespace ui;

void CounterApp::onDec(void* u)   { static_cast<CounterApp*>(u)->count_--; }
void CounterApp::onInc(void* u)   { static_cast<CounterApp*>(u)->count_++; }
void CounterApp::onReset(void* u) { static_cast<CounterApp*>(u)->confirmReset_ = true; }   // open modal
void CounterApp::onYes(void* u)   { auto* s = static_cast<CounterApp*>(u); s->count_ = 0; s->confirmReset_ = false; }
void CounterApp::onNo(void* u)    { static_cast<CounterApp*>(u)->confirmReset_ = false; }

bool CounterApp::onKey(Key k, AppContext&) {
    if (k == Key::Cancel && confirmReset_) { confirmReset_ = false; return true; }  // dismiss modal
    return false;   // otherwise Cancel exits the app
}

UiNode* CounterApp::buildModal(NodeArena& a, AppContext&) {
    if (!confirmReset_) return nullptr;
    Style row; row.dir = FlexDir::Row; row.gap = 8; row.align = Align::Center; row.justify = Justify::Center;
    return Modal(a, {
        Text(a, "Reset to zero?", TextRole::Body),
        Row(a, row, {
            Button(a, "Yes", onYes, this),
            Button(a, "No",  onNo,  this),
        }),
    });
}

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
