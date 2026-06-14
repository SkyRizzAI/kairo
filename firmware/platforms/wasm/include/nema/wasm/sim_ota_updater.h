#pragma once
#include "nema/hal/ota.h"

namespace nema {

// SimOtaUpdater — a DRY-RUN IOtaUpdater for the WASM simulator (Plan 39). WASM has
// no flash/slots, so this never swaps anything real: it accepts the streamed image
// (counts bytes) and reports success so the whole OTA pipeline — Forge "Update
// firmware" UI, the PLP Ota protocol, chunk flow-control, progress — can be
// exercised in the browser before touching hardware. commit() does NOT reboot.
class SimOtaUpdater : public IOtaUpdater {
public:
    bool        supported() const override { return true; }   // expose the flow
    bool        begin(uint32_t) override { written_ = 0; active_ = true; return true; }
    bool        write(const uint8_t*, size_t len) override {
        if (!active_) return false;
        written_ += (uint32_t)len;
        return true;
    }
    bool        commit() override { active_ = false; return true; }  // no real swap
    void        abort() override { active_ = false; written_ = 0; }
    uint32_t    written() const override { return written_; }
    const char* runningSlot() const override { return "sim (no real OTA)"; }

private:
    uint32_t written_ = 0;
    bool     active_  = false;
};

} // namespace nema
