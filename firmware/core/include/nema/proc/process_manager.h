#pragma once
#include <memory>
#include <vector>

// Plan 54 — ProcessManager: tracks live ProcessHost instances.
// Lightweight table: processes are short-lived (run-to-exit, single foreground).
// Background jobs (&) and job control are non-goals for now.

namespace nema {

class ProcessHost;

class ProcessManager {
public:
    // Register a running process. The manager does NOT own the host — caller
    // is responsible for lifetime (typically a stack or unique_ptr in the shell).
    void add(ProcessHost& host);
    void remove(ProcessHost& host);

    // List of currently-registered processes (for `ps`).
    const std::vector<ProcessHost*>& list() const { return hosts_; }

private:
    std::vector<ProcessHost*> hosts_;
};

} // namespace nema
