#include "nema/apps/task_demo_app.h"
#include "nema/app/app_context.h"
#include "nema/runtime.h"
#include "nema/clock.h"
#include "nema/task_runner.h"
#include "nema/thread.h"
#include "nema/ui/widgets.h"
#include <cstdio>

namespace nema {

using namespace ui;

bool TaskDemoApp::onKey(Key k, AppContext& ctx) {
    if (k == Key::Select && state_ != State::Working) {
        state_   = State::Working;
        startMs_ = ctx.runtime().clock().millis();
        finished_ = std::make_shared<std::atomic<bool>>(false);
        auto fin = finished_;
        ctx.runtime().tasks().submit(
            [] { nema::Thread::sleepMs(3000); },     // worker thread (blocks)
            [fin] { fin->store(true); });            // UI thread → flag
        return true;
    }
    return false;   // Cancel → base exits
}

bool TaskDemoApp::onTick(AppContext& ctx) {
    if (state_ != State::Working) return false;
    if (finished_ && finished_->load()) {
        lastMs_ = (uint32_t)(ctx.runtime().clock().millis() - startMs_);
        state_  = State::Done;
        doneCount_++;
        finished_.reset();
    }
    return true;   // animate elapsed while working (and the transition to Done)
}

UiNode* TaskDemoApp::build(NodeArena& a, AppContext& ctx) {
    if (state_ == State::Idle) {
        std::snprintf(line_, sizeof(line_), "Idle — press OK");
    } else if (state_ == State::Working) {
        uint32_t el = (uint32_t)(ctx.runtime().clock().millis() - startMs_);
        std::snprintf(line_, sizeof(line_), "Working... %u.%us",
                      (unsigned)(el / 1000), (unsigned)((el % 1000) / 100));
    } else {
        std::snprintf(line_, sizeof(line_), "Done in %ums", (unsigned)lastMs_);
    }
    std::snprintf(jobs_, sizeof(jobs_), "jobs done: %d", doneCount_);

    Style root;
    root.dir = FlexDir::Col; root.flexGrow = 1; root.padding = 4; root.gap = 8;
    root.align = Align::Center; root.justify = Justify::Center;

    return View(a, root, {
        Text(a, "Task Demo", TextRole::Caption),
        Text(a, line_, TextRole::Body),
        Text(a, jobs_, TextRole::Caption),
    });
}

} // namespace nema
