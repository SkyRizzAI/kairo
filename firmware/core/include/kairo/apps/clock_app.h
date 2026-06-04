#pragma once
#include "kairo/app/component_app.h"

namespace kairo {

// ClockApp — realtime clock on the component system (Plan 27). Display-only
// (no focusable widgets); periodic onTick redraws when the second changes.
class ClockApp : public ComponentApp {
public:
    const char* id()   const override { return "com.kairo.clock"; }
    const char* name() const override { return "Clock"; }

protected:
    ui::UiNode* build(ui::NodeArena& a, AppContext& ctx) override;
    uint32_t    tickIntervalMs() const override { return 250; }  // catch second rollover
    bool        onTick(AppContext& ctx) override;

private:
    int  lastSec_ = -1;
    char timeBuf_[12] = "";
    char dateBuf_[32] = "";
    void snapshot();   // refresh buffers from current time
};

} // namespace kairo
