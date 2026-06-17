// Plan 54 — ProcessManager implementation.
#include "nema/proc/process_manager.h"
#include "nema/proc/process_host.h"
#include <algorithm>

namespace nema {

void ProcessManager::add(ProcessHost& host) {
    hosts_.push_back(&host);
}

void ProcessManager::remove(ProcessHost& host) {
    hosts_.erase(std::remove(hosts_.begin(), hosts_.end(), &host), hosts_.end());
}

} // namespace nema
