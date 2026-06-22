#pragma once
#include "nema/ui/surface.h"
#include "nema/proc/process_context.h"
#include "nema/fs/app_storage.h"
#include <optional>

// Plan 54/55 — AppContext: the full process + surface interface an app gets.
//
// Inherits ISurface (canvas/present/nextInput/waitInput — Plan 55) and
// ProcessContext (args/cwd/env/stdio/exit/runtime — Plan 54). AppHost
// implements both sides; apps see a single AppContext& that covers everything.

namespace nema {

class Canvas;
class Runtime;

class AppContext : public ISurface, public ProcessContext {
public:
    // ISurface methods (canvas, present, nextInput, waitInput) are
    // inherited as pure virtuals — AppHost implements all four.

    // ── ProcessContext methods ────────────────────────────────────────
    // requestExit/exitCode (with code) are now on ProcessContext.
    // Use requestExit(code) instead of the old zero-arg requestExit().
    using ProcessContext::requestExit;

    // ── App identity ─────────────────────────────────────────────────
    // Bundle ID of the running app ("com.palanu.clock").
    // Implemented by AppHost; used to namespace storage and logs.
    virtual const char* bundleId() const = 0;

    // ── Namespaced storage (Plan 83) ─────────────────────────────────
    // Returns an AppStorage scoped to this app's bundle ID. The storage object
    // is cached — construction (which reads NVS) happens once on an
    // internal-RAM thread via warmStorage(), called by AppHost before the
    // PSRAM-stacked app thread starts.
    AppStorage& storage();
    AppStorage& criticalStorage();

    // Pre-constructs both storage objects on the calling thread (must be an
    // internal-RAM thread). AppHost::onResume() calls this before thread_.start().
    void warmStorage();

private:
    mutable std::optional<AppStorage> storage_;
    mutable std::optional<AppStorage> critStorage_;
};

} // namespace nema
