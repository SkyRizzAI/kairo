// Nema Thread — host (simulator) implementation via std::thread.
#include "kairo/nema/thread.h"
#include <thread>
#include <chrono>

namespace kairo::nema {

void Thread::start(const ThreadConfig& /*cfg*/, Entry entry, void* arg) {
    entry_ = entry;
    arg_   = arg;
    stop_.store(false);
    running_.store(true);
    os_ = new std::thread([this] {
        entry_(arg_);
        running_.store(false);
    });
}

void Thread::requestStop() { stop_.store(true); }

void Thread::join() {
    if (!os_) return;
    auto* t = static_cast<std::thread*>(os_);
    if (t->joinable()) t->join();
    delete t;
    os_ = nullptr;
}

void Thread::trampoline(void* /*self*/) {}  // unused on host

void Thread::sleepMs(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

Thread::~Thread() {
    requestStop();
    join();
}

} // namespace kairo::nema
