#include "nema/services/led_service.h"
#include "nema/log/logger.h"
#include <cstdio>

namespace nema {

// Small helper: "r,g,b" for logs.
static std::string rgbStr(uint8_t r, uint8_t g, uint8_t b) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u,%u,%u", (unsigned)r, (unsigned)g, (unsigned)b);
    return buf;
}

void LedService::addLed(ILed* led, const char* id, const char* desc) {
    if (!led) return;
    leds_.push_back({led, id ? id : "", desc ? desc : ""});
    fx_.push_back(Fx{});   // starts off/solid-black, dirty → cleared on first tick
}

// Resolve ledIdx (-1 = all) into a [begin,end) range, then run `fn(i)`.
template <typename Fn>
static void forEach(int count, int ledIdx, Fn fn) {
    if (ledIdx < 0) { for (int i = 0; i < count; i++) fn((size_t)i); }
    else if (ledIdx < count)          fn((size_t)ledIdx);
}

void LedService::solid(int ledIdx, uint8_t r, uint8_t g, uint8_t b) {
    if (log_) log_->info("LedService", "solid",
        {{"idx", std::to_string(ledIdx)}, {"rgb", rgbStr(r, g, b)}, {"leds", std::to_string(count())}});
    forEach(count(), ledIdx, [&](size_t i) {
        fx_[i] = Fx{r, g, b, 0, 0, -1, 0, true, true};
    });
}

void LedService::off(int ledIdx) { solid(ledIdx, 0, 0, 0); }

void LedService::blink(int ledIdx, uint8_t r, uint8_t g, uint8_t b,
                       uint16_t onMs, uint16_t offMs, int cycles) {
    if (log_) log_->info("LedService", "blink",
        {{"idx", std::to_string(ledIdx)}, {"rgb", rgbStr(r, g, b)},
         {"on", std::to_string(onMs)}, {"off", std::to_string(offMs)}});
    forEach(count(), ledIdx, [&](size_t i) {
        Fx f;
        f.r = r; f.g = g; f.b = b;
        f.onMs = onMs; f.offMs = offMs;
        f.cycles = cycles;
        f.phaseStart = 0;   // tick() seeds it on first pass
        f.lit = true;
        f.dirty = true;
        fx_[i] = f;
    });
}

void LedService::notify(Notify n, int ledIdx) {
    if (log_) log_->info("LedService", "notify", {{"intent", std::to_string((int)n)}});
    switch (n) {
        case Notify::Off:      off(ledIdx);                                   break;
        case Notify::Working:  blink(ledIdx,   0,   0, 255, 200, 200, -1);    break; // blue pulse
        case Notify::Success:  blink(ledIdx,   0, 255,   0, 120, 120,  3);    break; // green x3 → off
        case Notify::Error:    blink(ledIdx, 255,   0,   0, 100, 100,  6);    break; // red fast x6
        case Notify::Charging: blink(ledIdx, 255, 160,   0, 700, 700, -1);    break; // amber slow
    }
}

void LedService::applyOne(size_t i, bool lit) {
    ILed* l = leds_[i].led;
    if (!l) return;
    const Fx& f = fx_[i];
    if (lit) l->setAll(f.r, f.g, f.b);
    else     l->clear();
    l->show();
}

void LedService::tick(uint64_t nowMs) {
    for (size_t i = 0; i < fx_.size(); i++) {
        Fx& f = fx_[i];

        // Solid: apply once when dirty, then idle.
        if (f.onMs == 0 && f.offMs == 0) {
            if (f.dirty) { applyOne(i, true); f.dirty = false; }
            continue;
        }

        // Blink: seed timing on first pass, then toggle phase on schedule.
        if (f.dirty) { f.phaseStart = nowMs; f.lit = true; applyOne(i, true); f.dirty = false; continue; }

        uint16_t dur = f.lit ? f.onMs : f.offMs;
        if (nowMs - f.phaseStart < dur) continue;

        // Phase elapsed → flip.
        if (f.lit) {
            // finished an ON phase; count a cycle if bounded.
            if (f.cycles > 0 && --f.cycles == 0) {
                f.lit = false; f.onMs = f.offMs = 0;   // done → settle off (solid black)
                applyOne(i, false);
                continue;
            }
            f.lit = false;
        } else {
            f.lit = true;
        }
        f.phaseStart = nowMs;
        applyOne(i, f.lit);
    }
}

void LedService::stop() {
    for (size_t i = 0; i < leds_.size(); i++)
        if (leds_[i].led) { leds_[i].led->clear(); leds_[i].led->show(); }
}

} // namespace nema
