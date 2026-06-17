#pragma once
#include <cstddef>
#include <cstdint>

// Plan 50 — UI SDK descriptor + binding host interface.
// Each display server exposes its own UI SDK with a namespace.
// Only Aether currently has an SDK; FbconServer (headless-surface) returns nullptr.

namespace nema {

// Descriptor for one UI SDK exposed by a display server.
// App loaders check this to wire the correct imports per runtime.
struct UiSdkDescriptor {
    const char* ns;               // namespace, e.g. "aether:ui"
    uint16_t    versionMajor;     // bump = breaking change
    uint16_t    versionMinor;     // bump = additive
    const char* const* requiredCaps;  // capabilities this SDK needs, e.g. {"display", "input.2d"}
    size_t             requiredCapCount;
};

// Thin host-side registration interface used by a display server to wire
// its UI SDK functions into a runtime's import table (WASM/JS).
// C built-in apps don't use this — they link directly via generated headers.
struct IUiBindingHost {
    virtual ~IUiBindingHost() = default;

    // Register one function: namespace "aether:ui", function name "text", pointer to host C function.
    virtual void bind(const char* ns, const char* fn, void* hostFn) = 0;
};

} // namespace nema
