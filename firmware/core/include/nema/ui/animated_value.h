#pragma once
// AnimatedValue — spring-physics scalar for smooth UI transitions.
// Plan 90 F4.2
//
// Usage:
//   AnimatedValue scroll;
//   scroll.animateTo(targetY);           // kick toward target
//   if (scroll.tick(dtMs)) requestRedraw(); // call in screen tick()
//   float y = scroll.value;             // read current position
//
// Suitable for scroll positions, progress bars, slide offsets.
// NOT for 1-bit opacity blending — use the Transition system (F4.1) for that.

#include <cmath>

namespace aether::ui {

struct AnimatedValue {
    float value    = 0.f;
    float target   = 0.f;
    float velocity = 0.f;
    float stiffness = 200.f;
    float damping   =  20.f;

    static constexpr float kEpsilon = 0.5f;

    void animateTo(float t, float k = 200.f, float b = 20.f) {
        target    = t;
        stiffness = k;
        damping   = b;
    }

    void snapTo(float v) { value = target = v; velocity = 0.f; }

    bool isSettled() const {
        return std::fabs(target - value) < kEpsilon
            && std::fabs(velocity) < kEpsilon;
    }

    // Advance by dtMs milliseconds. Returns true while still animating.
    bool tick(float dtMs) {
        if (isSettled()) { value = target; velocity = 0.f; return false; }
        float dt  = dtMs * 0.001f;
        float acc = stiffness * (target - value) - damping * velocity;
        velocity += acc * dt;
        value    += velocity * dt;
        return true;
    }
};

} // namespace aether::ui
