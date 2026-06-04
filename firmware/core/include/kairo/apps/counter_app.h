#pragma once
#include "kairo/app/component_app.h"

namespace kairo {

// CounterApp — rewritten on the component system (Plan 27). Declarative tree:
// a value display + [−] [+] [Reset] buttons with focus navigation. No manual
// coordinates, no cursor handling — the layout engine + focus do it.
class CounterApp : public ComponentApp {
public:
    const char* id()   const override { return "com.kairo.counter"; }
    const char* name() const override { return "Counter"; }

protected:
    ui::UiNode* build(ui::NodeArena& a, AppContext& ctx) override;

private:
    int  count_ = 0;
    char buf_[16] = "0";

    static void onDec(void* u);
    static void onInc(void* u);
    static void onReset(void* u);
};

} // namespace kairo
