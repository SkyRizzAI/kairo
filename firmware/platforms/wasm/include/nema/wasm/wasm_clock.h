#pragma once
#include "nema/clock.h"
#include <emscripten.h>

namespace nema {

class WasmClock : public IClock {
    double start_;
public:
    WasmClock() : start_(emscripten_get_now()) {}
    uint64_t millis() override { return (uint64_t)(emscripten_get_now() - start_); }
    uint64_t epochMs() override { return (uint64_t)emscripten_get_now(); }
};

} // namespace nema
