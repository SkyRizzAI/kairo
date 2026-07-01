#pragma once
#include "nema/service.h"
#include "nema/hal/led.h"
#include <cstdint>
#include <string>
#include <vector>

namespace nema {

class Logger;

// LedService — the runtime LED registry + effect engine (`rt.led()`).
//
// Multi-instance like AudioService: a board registers each physical LED / strip
// (addLed) and everything downstream (settings, apps) enumerates them. The
// effect engine (solid / blink / notification intents) runs non-blocking in
// tick() and drives the low-level ILed drivers — so an app can ask for "blink
// while reading" and walk away; the service keeps it going.
//
// Board-agnostic: `notify()` takes an INTENT (Working/Success/Error/…), not a
// colour, so a mono LED degrades to a blink pattern and a board with no LED is a
// no-op. Apps that want literal colours use solid()/blink().
class LedService : public IService {
public:
    // ── Registry ──
    void        addLed(ILed* led, const char* id, const char* desc);
    int         count() const { return (int)leds_.size(); }
    ILed*       led(int i)          { return (i >= 0 && i < count()) ? leds_[(size_t)i].led : nullptr; }
    const char* id(int i)   const   { return (i >= 0 && i < count()) ? leds_[(size_t)i].id.c_str()   : ""; }
    const char* desc(int i) const   { return (i >= 0 && i < count()) ? leds_[(size_t)i].desc.c_str() : ""; }

    // ── Effects (non-blocking; ledIdx -1 = all LEDs) ──
    void solid(int ledIdx, uint8_t r, uint8_t g, uint8_t b);
    void off  (int ledIdx);
    void blink(int ledIdx, uint8_t r, uint8_t g, uint8_t b,
               uint16_t onMs, uint16_t offMs, int cycles = -1);   // -1 = forever

    // ── Notification intents (board-agnostic) ──
    enum class Notify : uint8_t { Off, Working, Success, Error, Charging };
    void notify(Notify n, int ledIdx = -1);

    // Optional logger so effect calls are observable (set by Runtime at adopt).
    void setLogger(Logger* lg) { log_ = lg; }

    // ── IService ──
    const char* name() const override { return "LedService"; }
    void start() override {}
    void stop()  override;
    void tick(uint64_t nowMs) override;

private:
    struct Entry { ILed* led; std::string id; std::string desc; };
    struct Fx {
        uint8_t  r = 0, g = 0, b = 0;   // colour
        uint16_t onMs = 0, offMs = 0;   // both 0 = solid
        int      cycles = -1;           // remaining on-phases; -1 = forever
        uint64_t phaseStart = 0;
        bool     lit = false;           // current phase state
        bool     dirty = true;          // needs re-apply to hardware
    };
    void applyOne(size_t i, bool lit);

    std::vector<Entry> leds_;
    std::vector<Fx>    fx_;             // parallel to leds_
    Logger*            log_ = nullptr;
};

} // namespace nema
