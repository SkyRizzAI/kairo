#include "kairo/sim/mem_config_store.h"
#include <cstdio>

namespace kairo {

bool MemConfigStore::getString(const char* ns, const char* key, std::string& out) const {
    auto ni = store_.find(ns);
    if (ni == store_.end()) return false;
    auto ki = ni->second.find(key);
    if (ki == ni->second.end()) return false;
    out = ki->second;
    return true;
}

bool MemConfigStore::getInt(const char* ns, const char* key, int64_t& out) const {
    std::string v;
    if (!getString(ns, key, v)) return false;
    try { out = std::stoll(v); return true; }
    catch (...) { return false; }
}

void MemConfigStore::setString(const char* ns, const char* key, const std::string& val) {
    store_[ns][key] = val;
}

void MemConfigStore::setInt(const char* ns, const char* key, int64_t val) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld", (long long)val);
    store_[ns][key] = buf;
}

bool MemConfigStore::remove(const char* ns, const char* key) {
    auto ni = store_.find(ns);
    if (ni == store_.end()) return false;
    return ni->second.erase(key) > 0;
}

} // namespace kairo
