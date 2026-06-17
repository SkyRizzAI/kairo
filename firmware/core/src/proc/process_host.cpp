// Plan 54 — ProcessHost implementation.
#include "nema/proc/process_host.h"
#include "nema/app/app.h"
#include "nema/runtime.h"
#include <utility>

namespace nema {

ProcessHost::ProcessHost(Runtime& rt, IApp& app, ProcessSpec spec)
    : rt_(rt), app_(app), spec_(std::move(spec)) {}

ProcessHost::~ProcessHost() {
    if (thread_.running()) {
        requestExit(1);
        thread_.join();
    }
}

const char* ProcessHost::env(const char* /*key*/) const {
    // No env support yet — always null.
    return nullptr;
}

IInputStream& ProcessHost::in() {
    return spec_.stdin_ ? *spec_.stdin_ : nullIn_;
}

IOutputStream& ProcessHost::out() {
    return spec_.stdout_ ? *spec_.stdout_ : nullOut_;
}

IOutputStream& ProcessHost::err() {
    return spec_.stderr_ ? *spec_.stderr_ : nullOut_;
}

void ProcessHost::requestExit(int code) {
    exitCode_.store(code);
    exitReq_.store(true);
    thread_.requestStop();
}

bool ProcessHost::shouldExit() const {
    return exitReq_.load() || thread_.shouldStop();
}

int ProcessHost::exitCode() const {
    return exitCode_.load();
}

Runtime& ProcessHost::runtime() {
    return rt_;
}

void ProcessHost::start() {
    thread_.start({"nema_app", app_.stackBytes(), 5, 0}, &ProcessHost::threadEntry, this);
}

bool ProcessHost::finished() const {
    return !thread_.running();
}

int ProcessHost::join() {
    thread_.join();
    return exitCode_.load();
}

void ProcessHost::threadEntry(void* self) {
    auto* h = static_cast<ProcessHost*>(self);
    h->app_.runProcess(*h);   // headless entry point (Plan 54)
}

} // namespace nema
