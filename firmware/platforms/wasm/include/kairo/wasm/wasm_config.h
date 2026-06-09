#pragma once
#include "kairo/config/config_store.h"
#include <map>
#include <string>

namespace kairo {

// Minimal in-memory config for the WASM device (browser localStorage bridge can
// be added later). Enough for the runtime + apps to boot.
class WasmConfig : public IConfigStore {
public:
    bool getString(const char* ns, const char* key, std::string& out) const override {
        auto it = s_.find(k(ns, key));
        if (it == s_.end()) return false;
        out = it->second;
        return true;
    }
    bool getInt(const char* ns, const char* key, int64_t& out) const override {
        auto it = i_.find(k(ns, key));
        if (it == i_.end()) return false;
        out = it->second;
        return true;
    }
    void setString(const char* ns, const char* key, const std::string& v) override { s_[k(ns, key)] = v; }
    void setInt(const char* ns, const char* key, int64_t v) override { i_[k(ns, key)] = v; }
    bool remove(const char* ns, const char* key) override {
        return s_.erase(k(ns, key)) || i_.erase(k(ns, key));
    }

private:
    static std::string k(const char* ns, const char* key) {
        return std::string(ns) + "/" + key;
    }
    std::map<std::string, std::string> s_;
    std::map<std::string, int64_t>     i_;
};

} // namespace kairo
