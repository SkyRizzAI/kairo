#pragma once
#include "kairo/nema/thread.h"
#include "kairo/nema/message_queue.h"
#include <functional>

namespace kairo::nema {

// TaskRunner — offload blocking work off the UI thread.
//
// THIS is what makes Kairo "never freeze": a plugin/screen that needs to do
// something slow (HTTP download, file I/O, crypto, long compute) submits it as
// a Job. The Job runs on a dedicated worker thread — the UI thread keeps
// ticking, rendering, and handling input the whole time. When the Job finishes,
// its optional `done` callback runs back ON THE UI THREAD (drained in
// Runtime::step), so it can safely touch UI / plugin state with no locks.
//
//   rt.tasks().submit(
//       [shared] { shared->result = http.get(url); },   // worker thread (blocks)
//       [shared] { screen.show(shared->result); });      // UI thread (safe)
//
// v1: a single serial worker (one Job at a time). A pool can come later; the
// API does not change. Jobs are FIFO.
class TaskRunner {
public:
    using Job  = std::function<void()>;   // runs on worker thread; MAY block
    using Done = std::function<void()>;   // runs on UI thread after Job finishes

    void start(uint32_t stackBytes = 8192);  // spawn worker thread
    void stop();                             // stop & join worker

    // Submit a job. `done` (optional) runs on the UI thread once the job
    // completes. Thread-safe — call from any thread. Returns false if the
    // job queue is full (job not accepted).
    bool submit(Job job, Done done = {});

    // UI thread: invoke completion callbacks for finished jobs. Call each frame.
    void drainCompletions();

private:
    struct Task { Job job; Done done; };
    static void workerEntry(void* self);
    void        worker();

    Thread             thread_;
    MessageQueue<Task> jobs_{16};         // UI → worker
    MessageQueue<Done> completions_{16};  // worker → UI
};

} // namespace kairo::nema
