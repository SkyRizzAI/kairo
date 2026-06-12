// Nema Thread — host (simulator) implementation via std::thread.
//
// Stack sizing note: std::thread does not let us set a per-thread stack size, so
// the JS app's stackBytes() is not honoured here the way ESP's xTaskCreate honours
// it. Instead the thread stack is configured platform-wide:
//   - native host:  the libc default (≈512 KB on macOS/Linux) — ample.
//   - WASM:         set via -sDEFAULT_PTHREAD_STACK_SIZE in targets/wasm (the
//                   Emscripten default is only 64 KB, far too small for QuickJS).
// The QuickJS recursion guard (JsApp::onStart → setMaxStackSize) is kept safely
// below that real stack, so a runaway script throws a clean error rather than
// overflowing.
#include "nema/thread.h"
#include <thread>
#include <chrono>

namespace nema {

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

} // namespace nema
