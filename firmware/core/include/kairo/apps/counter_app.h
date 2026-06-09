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
    ui::UiNode* buildModal(ui::NodeArena& a, AppContext& ctx) override;
    bool        onKey(Key k, AppContext& ctx) override;

private:
    int  count_ = 0;
    char buf_[16] = "0";
    bool confirmReset_ = false;   // shows the Yes/No reset modal

    static void onDec(void* u);
    static void onInc(void* u);
    static void onReset(void* u);   // opens the confirm modal
    static void onYes(void* u);     // modal: confirm reset
    static void onNo(void* u);      // modal: dismiss
};

} // namespace kairo
