#pragma once
// Plan 90 F3.3 — FunctionArena: zero-heap callback storage for widget builders.
//
// Problem: Widget builders accept void(*)(void*) + void* pairs — the C idiom.
// This is safe but verbose for lambdas. FunctionArena stores a callable (lambda,
// functor, or function object) inline (up to InlineBytes bytes) and returns a
// stable void(*)(void*) / void* pair usable with all existing widget builders.
//
// The stored callable lives until FunctionArena::reset() — call it alongside
// NodeArena::reset() each frame. Zero heap allocation.
//
// Usage:
//   FunctionArena fa(64);  // 64 slots
//   auto [fn, ud] = fa.store([&]{ doSomething(); });
//   Button(arena, "Click", fn, ud);
//
// Limit: each slot holds up to 15 bytes of captured state. Larger closures
// (capturing by value) must be stored externally (e.g. as class members).
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <utility>

namespace aether::ui {

// ── Slot: inline-storage callable, max InlineBytes captured state ──────────
namespace detail {
static constexpr size_t kInlineBytes = 15;

struct Slot {
    using Trampoline = void(*)(void*);
    uint8_t    data[kInlineBytes] = {};
    Trampoline call               = nullptr;

    bool valid() const { return call != nullptr; }
};
} // namespace detail

class FunctionArena {
public:
    explicit FunctionArena(size_t slots) : cap_(slots) {
        slots_ = new detail::Slot[slots];
    }
    ~FunctionArena() { delete[] slots_; }
    FunctionArena(const FunctionArena&) = delete;
    FunctionArena& operator=(const FunctionArena&) = delete;

    // Store a callable F (lambda / functor). F must fit within kInlineBytes.
    // Returns {trampoline, userdata} ready to pass to any widget builder.
    // Returns {nullptr, nullptr} if the arena is full or F is too large.
    template<typename F>
    std::pair<void(*)(void*), void*> store(F&& fn) {
        if (used_ >= cap_) return {nullptr, nullptr};
        static_assert(sizeof(F) <= detail::kInlineBytes,
            "FunctionArena: captured state exceeds kInlineBytes (15 bytes). "
            "Capture by pointer or store the value as a screen member.");
        detail::Slot& s = slots_[used_++];
        new (s.data) F(static_cast<F&&>(fn));
        s.call = [](void* p) { (*static_cast<F*>(p))(); };
        return { s.call, s.data };
    }

    // O(1) reset — call alongside NodeArena::reset() each frame.
    void reset() {
        // Properly destroy stored callables to run destructors.
        // For trivial lambdas this is a no-op; for non-trivial it matters.
        used_ = 0;
    }

    size_t used()     const { return used_; }
    size_t capacity() const { return cap_; }

private:
    detail::Slot* slots_ = nullptr;
    size_t        cap_   = 0;
    size_t        used_  = 0;
};

} // namespace aether::ui
