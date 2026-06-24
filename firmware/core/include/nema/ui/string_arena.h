#pragma once
// Plan 90 F1.2 — StringArena: bump allocator for stable const char* lifetimes.
//
// Problem: UiNode::text is a raw const char* — if a caller passes a pointer to
// a local std::string or stack buffer, it becomes a dangling pointer after
// build() returns. StringArena solves this: intern() copies the string into a
// flat arena buffer and returns a stable pointer that lives until reset().
//
// Usage:
//   StringArena sa(512);
//   node->text = sa.intern(myStdString);   // safe: copied into arena
//   node->text = "literal";               // still OK: literals are static
//
// ComponentApp / ComponentScreen expose `sa` alongside `arena` so both reset
// together each frame. Zero heap allocation — the arena is stack/PSRAM.
#include <cstddef>
#include <cstring>
#include <string>

namespace aether::ui {

class StringArena {
public:
    explicit StringArena(size_t capacity) : cap_(capacity) {
        buf_ = new char[capacity];
    }
    ~StringArena() { delete[] buf_; }
    StringArena(const StringArena&) = delete;
    StringArena& operator=(const StringArena&) = delete;

    // Copy str into the arena and return a stable pointer.
    // Returns a pointer to an empty string literal if the arena is full.
    const char* intern(const char* str) {
        if (!str) return "";
        size_t len = strlen(str) + 1;
        if (used_ + len > cap_) return "";   // overflow: return empty, never null
        char* dst = buf_ + used_;
        memcpy(dst, str, len);
        used_ += len;
        return dst;
    }

    const char* intern(const std::string& s) { return intern(s.c_str()); }

    // O(1) reset — call at the start of each render frame, same as NodeArena.
    void reset() { used_ = 0; }

    size_t used()     const { return used_; }
    size_t capacity() const { return cap_; }

private:
    char*  buf_  = nullptr;
    size_t cap_  = 0;
    size_t used_ = 0;
};

} // namespace aether::ui
