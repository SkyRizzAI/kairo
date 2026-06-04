#pragma once
#include "kairo/app/component_app.h"
#include <cstdint>

namespace kairo {

// StopwatchApp — on the component system (Plan 27). Display-only tree; Select
// toggles run/stop, Up resets (when stopped), Cancel exits. onTick animates
// while running (~20fps).
class StopwatchApp : public ComponentApp {
public:
    const char* id()   const override { return "com.kairo.stopwatch"; }
    const char* name() const override { return "Stopwatch"; }
    bool fullscreen()  const override { return true; }

protected:
    ui::UiNode* build(ui::NodeArena& a, AppContext& ctx) override;
    bool        onKey(Key k, AppContext& ctx) override;
    uint32_t    tickIntervalMs() const override { return running_ ? 50 : 200; }
    bool        onTick(AppContext&) override { return running_; }

private:
    bool     running_ = false;
    uint64_t startMs_ = 0;
    uint64_t elapsed_ = 0;   // accumulated before current run
    char     timeBuf_[20] = "00:00.000";

    uint64_t totalMs(AppContext& ctx) const;
};

} // namespace kairo
