#include "nema/services/permission_service.h"
#include "nema/runtime.h"
#include "nema/config/config_store.h"
#include "nema/ui/view_dispatcher.h"
#include <cstdio>

namespace nema {

// djb2(appId + ":" + cap) → 8-char hex. Fits NVS 15-char key limit even for
// long bundle IDs ("com.palanu.badusb") and long cap names ("net.wifi.monitor").
static std::string permKey(const std::string& appId, const std::string& cap) {
    uint32_t h = 5381;
    for (unsigned char c : appId) h = ((h << 5) + h) + c;
    h = ((h << 5) + h) + ':';
    for (unsigned char c : cap)   h = ((h << 5) + h) + c;
    char buf[9];
    snprintf(buf, sizeof(buf), "%08x", (unsigned int)h);
    return buf;
}

void PermissionService::init(Runtime& rt) {
    rt_ = &rt;
}

uint8_t PermissionService::status(const std::string& appId,
                                   const std::string& cap) const {
    if (!rt_) return 0;
    int64_t v = 0;
    if (rt_->config().getInt("perm", permKey(appId, cap).c_str(), v))
        return static_cast<uint8_t>(v);
    return 0;  // not_asked
}

uint8_t PermissionService::request(const std::string& appId,
                                    const std::string& cap) {
    uint8_t s = status(appId, cap);
    if (s != 0) return s;  // already decided

    auto req = std::make_shared<PermRequest>();
    req->appId = appId;
    req->cap   = cap;

    {
        std::lock_guard<std::mutex> g(mu_);
        pending_      = req;
        screenPushed_ = false;
    }

    // Block until the GUI thread pushes the screen and the user decides.
    std::unique_lock<std::mutex> lock(req->mu);
    req->cv.wait(lock, [&req] { return req->done; });

    persist(appId, cap, req->result);

    {
        std::lock_guard<std::mutex> g(mu_);
        if (pending_ == req) {
            pending_.reset();
            screenPushed_ = false;
        }
    }

    return req->result;
}

void PermissionService::guiTick(ViewDispatcher& vd) {
    if (!factory_) return;

    std::shared_ptr<PermRequest> req;
    {
        std::lock_guard<std::mutex> g(mu_);
        if (!pending_ || screenPushed_) return;
        screenPushed_ = true;
        req = pending_;
    }

    // Factory runs on the GUI thread; it prepares the screen and calls
    // vd.navigate(*screen) to push it.
    factory_(req, vd);
}

void PermissionService::persist(const std::string& appId,
                                 const std::string& cap, uint8_t val) {
    if (!rt_) return;
    rt_->config().setInt("perm", permKey(appId, cap).c_str(),
                         static_cast<int64_t>(val));
}

} // namespace nema
