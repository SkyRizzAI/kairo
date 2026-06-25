#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

// ISeedStore — persistence sink for the encrypted seed blob (Plan 94, Fase 1).
// Decouples NvsBackend from the storage layer so it is host-testable: firmware
// backs this with AppStorage::critical() (internal flash only, never SD); host
// tests use an in-memory implementation.

namespace nema::wallet {

struct ISeedStore {
    virtual ~ISeedStore() = default;
    virtual bool save(const uint8_t* blob, size_t n) = 0;
    virtual bool load(std::vector<uint8_t>& out) = 0;
    virtual bool exists() const = 0;
    virtual void erase() = 0;
};

}  // namespace nema::wallet
