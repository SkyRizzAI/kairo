#pragma once
#include <string>
#include <cstdint>

namespace nema {

// IConfigStore — persistent key-value store.
//
// Namespace + key addressing. ESP32: backed by NVS (internal flash, 24 KB,
// wear-leveled). Simulator: in-memory std::map. Both share this interface.
//
// Constraints (NVS limits): namespace ≤ 15 chars, key ≤ 15 chars.
// Values: arbitrary-length strings, or signed 64-bit integers.
//
// Write operations commit immediately — no explicit flush needed.
class IConfigStore {
public:
    virtual ~IConfigStore() = default;

    // Service lifecycle — called by ServiceManager
    virtual void start() {}
    virtual void stop()  {}
    virtual void tick(uint64_t) {}

    // Read — returns false if the key does not exist (out is unchanged).
    virtual bool getString(const char* ns, const char* key, std::string& out) const = 0;
    virtual bool getInt   (const char* ns, const char* key, int64_t& out)     const = 0;

    // Write — always succeeds (asserts / logs on driver error)
    virtual void setString(const char* ns, const char* key, const std::string& val) = 0;
    virtual void setInt   (const char* ns, const char* key, int64_t val)            = 0;

    // Delete — returns true if the key existed
    virtual bool remove(const char* ns, const char* key) = 0;

    // Convenience helpers with default values.
    // getString overload: unambiguous (const char* != std::string&).
    // getInt helper is named getIntOr to avoid overload ambiguity (int64_t value vs int64_t&).
    std::string getString(const char* ns, const char* key, const char* def) const {
        std::string v;
        return getString(ns, key, v) ? v : def;
    }
    int64_t getIntOr(const char* ns, const char* key, int64_t def) const {
        int64_t v;
        return getInt(ns, key, v) ? v : def;
    }
};

} // namespace nema
