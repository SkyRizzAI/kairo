#pragma once
#include "nema/app/component_app.h"
#include <atomic>
#include <memory>
#include <cstdint>

namespace nema {

// TaskDemoApp — on the component system (Plan 27). OK submits a 3s "download" to
// a TaskRunner worker; the app polls completion in onTick and animates an elapsed
// counter while staying cancelable — nothing freezes.
class TaskDemoApp : public ComponentApp {
public:
    const char* id()   const override { return "com.palanu.taskdemo"; }
    const char* name() const override { return "Task Demo"; }

protected:
    ui::UiNode* build(ui::NodeArena& a, AppContext& ctx) override;
    bool        onKey(Key k, AppContext& ctx) override;       // Select = start job
    uint32_t    tickIntervalMs() const override { return 100; }
    bool        onTick(AppContext& ctx) override;

private:
    enum class State { Idle, Working, Done };
    State    state_     = State::Idle;
    int      doneCount_ = 0;
    uint64_t startMs_   = 0;
    uint32_t lastMs_    = 0;
    char     line_[40]  = "Idle";
    char     jobs_[24]  = "jobs done: 0";
    std::shared_ptr<std::atomic<bool>> finished_;
};

} // namespace nema
