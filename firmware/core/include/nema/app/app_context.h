#pragma once
#include "nema/ui/surface.h"
#include "nema/proc/process_context.h"

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
};

} // namespace nema
