#pragma once
#include "nema/ui/surface.h"
#include "nema/proc/process_context.h"
#include "nema/fs/app_storage.h"

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
    // Returns an AppStorage scoped to this app's bundle ID.
    // Prefers internal flash; routes to SD if user moved the app's data there.
    // Use criticalStorage() for credentials / private keys (always internal).
    AppStorage storage();
    AppStorage criticalStorage();
};

} // namespace nema
