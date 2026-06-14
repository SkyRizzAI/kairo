#include "nema/wasm/wasm_cable_transport.h"
#include <emscripten.h>
#include <cstdint>

namespace nema {

// Single transport instance (the device has one cable to the host).
static WasmCableTransport* g_cable = nullptr;

void WasmCableTransport::init() { g_cable = this; }

bool WasmCableTransport::send(const uint8_t* data, size_t len) {
    // send() may run on a pthread (e.g. the GUI/render thread on screen flush).
    // MAIN_THREAD_EM_ASM marshals the call to the module's main runtime thread,
    // where the JS layer installed Module.nemaPlpOut. Shared heap → pointer is
    // valid there too. In-process delivery (no postMessage / nested workers).
    MAIN_THREAD_EM_ASM({
        if (Module && Module.nemaPlpOut) Module.nemaPlpOut(HEAPU8.slice($0, $0 + $1));
    }, data, (int)len);
    return true;
}

// Inbound: the JS worker calls this (on the worker main thread) with a heap
// pointer + length when the host delivers PLP bytes.
extern "C" EMSCRIPTEN_KEEPALIVE void nema_plp_recv(const uint8_t* ptr, int len) {
    if (g_cable) g_cable->deliver(ptr, (size_t)len);
}

} // namespace nema
