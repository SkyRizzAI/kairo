#include "kairo/nema/task_runner.h"

namespace kairo::nema {

void TaskRunner::start(uint32_t stackBytes) {
    // Worker on core 0 (with WiFi/TLS), priority below UI. Jobs are I/O-bound
    // and yield; they won't starve WiFi. UI loop runs on core 1.
    thread_.start({"nema_worker", stackBytes, 4, 0}, &TaskRunner::workerEntry, this);
}

void TaskRunner::stop() {
    thread_.requestStop();
    thread_.join();
}

bool TaskRunner::submit(Job job, Done done) {
    return jobs_.send(Task{std::move(job), std::move(done)});
}

void TaskRunner::workerEntry(void* self) {
    static_cast<TaskRunner*>(self)->worker();
}

void TaskRunner::worker() {
    Task t;
    while (!thread_.shouldStop()) {
        // Block up to 100ms for a job, then loop to re-check shouldStop().
        if (!jobs_.receive(t, 100)) continue;
        if (t.job) t.job();                       // <-- the slow work, isolated
        if (t.done) completions_.send(std::move(t.done));  // hand result to UI
        t = Task{};                               // release captured state
    }
}

void TaskRunner::drainCompletions() {
    Done d;
    while (completions_.tryReceive(d)) {
        if (d) d();                               // runs on UI thread — safe
    }
}

} // namespace kairo::nema
