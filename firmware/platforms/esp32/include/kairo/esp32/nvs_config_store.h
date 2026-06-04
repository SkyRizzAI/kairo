#pragma once
#include "kairo/config/config_store.h"

namespace kairo {

class Logger;

// NvsConfigStore — IConfigStore backed by ESP-IDF NVS (internal flash).
// Thread-safe, wear-leveled, survives reboot. No SD card needed.
// Each operation opens the namespace, reads/writes, commits, and closes.
class NvsConfigStore : public IConfigStore {
public:
    void init(Logger& log) { log_ = &log; }

    void start() override;   // nvs_flash_init (erase + reinit on corruption)

    bool getString(const char* ns, const char* key, std::string& out) const override;
    bool getInt   (const char* ns, const char* key, int64_t& out)     const override;
    void setString(const char* ns, const char* key, const std::string& val) override;
    void setInt   (const char* ns, const char* key, int64_t val)            override;
    bool remove   (const char* ns, const char* key)                         override;

private:
    Logger* log_ = nullptr;
};

} // namespace kairo
