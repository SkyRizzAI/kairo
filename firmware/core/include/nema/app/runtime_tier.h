#pragma once
#include <cstdint>

// Plan 56 — RuntimeTier: the execution environment an installed app targets.
//
//   CBuiltin — Native C++ compiled into the firmware (IApp subclass).
//              Zero overhead; direct API access; no memory sandbox.
//   Js       — QuickJS interpreter sandbox (JsRuntime, Plan 58).
//              .kapp bundles; GC-managed; 4 MB heap cap; portable.
//   Wasm     — wasm3 interpreter sandbox (WasmRuntime, Plan 57).
//              .papp bundles; linear-memory isolation; WASI stdio mapping.
//
// The tier is stored on AppManifest so the loader, `ps`, and the launcher
// can show which runtime each app runs in without inspecting the bundle.

namespace nema {

enum class RuntimeTier : uint8_t {
    CBuiltin = 0,
    Js       = 1,
    Wasm     = 2,
};

inline const char* runtimeTierName(RuntimeTier t) {
    switch (t) {
        case RuntimeTier::CBuiltin: return "native";
        case RuntimeTier::Js:       return "js";
        case RuntimeTier::Wasm:     return "wasm";
    }
    return "?";
}

} // namespace nema
