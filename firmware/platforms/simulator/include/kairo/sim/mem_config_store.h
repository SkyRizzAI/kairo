#pragma once
#include "kairo/config/config_store.h"
#include <map>
#include <string>

namespace kairo {

// MemConfigStore — in-memory IConfigStore for the simulator.
// Values live for the duration of the process (not persisted to disk).
class MemConfigStore : public IConfigStore {
public:
    bool getString(const char* ns, const char* key, std::string& out) const override;
    bool getInt   (const char* ns, const char* key, int64_t& out)     const override;
    void setString(const char* ns, const char* key, const std::string& val) override;
    void setInt   (const char* ns, const char* key, int64_t val)            override;
    bool remove   (const char* ns, const char* key)                         override;

private:
    // map[namespace][key] = string value (ints stored as decimal strings)
    std::map<std::string, std::map<std::string, std::string>> store_;

    static std::string makeKey(const char* ns, const char* key);
};

} // namespace kairo
