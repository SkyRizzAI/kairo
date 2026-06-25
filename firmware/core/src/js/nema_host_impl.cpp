// nema_host_impl.cpp — Hand-written implementation of HostApi (Plan 49 Fase 2).
// This is the SINGLE file where C++ touches rt.* to back the System API.
// All QuickJS/WASM marshalling is generated; this file provides the semantics.
// Adding a function to the IDL without adding an impl here → build error.

#include "host/nema_api.gen.h"
#include "nema/runtime.h"
#include "nema/log/logger.h"
#include "nema/system/capability_registry.h"
#include "nema/system/system_info.h"
#include "nema/service/service_container.h"
#include "nema/config/config_store.h"
#include "nema/fs/app_storage.h"
#include "nema/app/app_context.h"
#include "nema/hal/http_client.h"
#include "nema/services/profile_service.h"
#include "nema/services/permission_service.h"
#include "nema/services/resource_broker.h"
#include "nema/hal/radio_wifi.h"
#include "nema/ui/aether_abi.h"
#include <string>
#include <utility>
#include <cstdint>

namespace {

// djb2 hash → 8-char hex string, always fits in NVS 15-char namespace limit.
// Bundle IDs like "com.palanu.badusb" (18 chars) would silently truncate in NVS
// without this — using the value directly was a silent hardware bug.
static std::string nvsNs(const std::string& bundleId) {
    uint32_t h = 5381;
    for (char c : bundleId) h = ((h << 5) + h) + (uint8_t)c;
    char buf[9];
    snprintf(buf, sizeof(buf), "%08x", (unsigned int)h);
    return buf;
}

class NemaHostImpl : public HostApi {
public:
    NemaHostImpl(nema::Runtime& rt, std::string appId, nema::AppContext* ctx = nullptr)
        : rt_(rt), appId_(std::move(appId)), ns_(nvsNs(appId_)), appCtx_(ctx) {}

    // ── nema:sys/log ──────────────────────────────────────────────────

    void log_log(std::string_view level, std::string_view tag, std::string_view msg) override {
        auto& log = rt_.log();
        if      (level == "error") log.error(tag.data(), msg.data());
        else if (level == "warn")  log.warn (tag.data(), msg.data());
        else if (level == "debug") log.debug(tag.data(), msg.data());
        else if (level == "trace") log.trace(tag.data(), msg.data());
        else if (level == "fatal") log.fatal(tag.data(), msg.data());
        else                       log.info (tag.data(), msg.data());
    }

    // ── nema:sys/device ───────────────────────────────────────────────

    std::string device_name() override {
        return rt_.info().boardName;
    }

    std::vector<std::string> device_caps() override {
        return rt_.capabilities().list();
    }

    bool device_has(std::string_view cap) override {
        return rt_.capabilities().has(std::string(cap));
    }

    bool device_available(std::string_view cap) override {
        return rt_.capabilities().available(std::string(cap));
    }

    // ── nema:sys/perm ─────────────────────────────────────────────────

    uint8_t perm_status(std::string_view cap) override {
        auto* perm = rt_.container().resolve<nema::PermissionService>();
        return perm ? perm->status(appId_, std::string(cap)) : 0;
    }

    uint8_t perm_request(std::string_view cap) override {
        auto* perm = rt_.container().resolve<nema::PermissionService>();
        if (!perm) return 1;  // no service = dev/headless mode, always grant
        return perm->request(appId_, std::string(cap));
    }

    bool perm_check(std::string_view /*app_id*/, std::string_view cap) override {
        auto* perm = rt_.container().resolve<nema::PermissionService>();
        if (!perm) return true;
        return perm->status(appId_, std::string(cap)) == 1;
    }

    // ── nema:sys/lease ────────────────────────────────────────────────

    NemaResult<uint32_t, LeaseError> lease_acquire(std::string_view cap) override {
        auto* broker = rt_.container().resolve<nema::ResourceBroker>();
        if (!broker) return {true, 0u, {}};  // no broker = dev mode, dummy handle
        return broker->acquire(appId_, std::string(cap));
    }

    NemaResult<void, std::string> lease_release(uint32_t handle) override {
        auto* broker = rt_.container().resolve<nema::ResourceBroker>();
        if (!broker) return {true, {}};
        return broker->release(appId_, handle);
    }

    bool lease_check(std::string_view /*app_id*/, std::string_view cap) override {
        auto* broker = rt_.container().resolve<nema::ResourceBroker>();
        if (!broker) return true;
        return broker->holdsLease(appId_, std::string(cap));
    }

    // ── nema:sys/events ───────────────────────────────────────────────
    // @future — not yet implemented; stubs log a warning.

    int32_t events_subscribe(std::string_view, int32_t) override { return -1; }
    void    events_unsubscribe(int32_t) override {}
    void    events_publish(std::string_view, const std::vector<Field>&) override {}

    // ── nema:sys/tasks ────────────────────────────────────────────────
    // @future

    void    tasks_submit(int32_t, int32_t) override {}
    int32_t tasks_timeout(uint32_t, int32_t) override { return -1; }
    int32_t tasks_interval(uint32_t, int32_t) override { return -1; }
    void    tasks_cancel(int32_t) override {}

    // ── nema:storage/kv ───────────────────────────────────────────────

    std::optional<std::string> kv_get(std::string_view key) override {
        std::string v;
        if (rt_.config().getString(ns_.c_str(), std::string(key).c_str(), v))
            return v;
        return std::nullopt;
    }

    void kv_set(std::string_view key, std::string_view value) override {
        rt_.config().setString(ns_.c_str(), std::string(key).c_str(), std::string(value));
    }

    std::optional<int64_t> kv_get_int(std::string_view key) override {
        int64_t v = 0;
        if (rt_.config().getInt(ns_.c_str(), std::string(key).c_str(), v))
            return v;
        return std::nullopt;
    }

    void kv_set_int(std::string_view key, int64_t value) override {
        rt_.config().setInt(ns_.c_str(), std::string(key).c_str(), value);
    }

    bool kv_remove(std::string_view key) override {
        return rt_.config().remove(ns_.c_str(), std::string(key).c_str());
    }

    // ── nema:storage/fs ───────────────────────────────────────────────

    std::optional<std::string> fs_read_file(std::string_view name) override {
        std::vector<uint8_t> buf;
        bool ok = storRef().read(std::string(name).c_str(), buf);
        rt_.log().info("AppStorage", ok ? "read ok" : "read miss",
                       {{"app", appId_}, {"file", std::string(name)}});
        if (!ok) return std::nullopt;
        return std::string(buf.begin(), buf.end());
    }

    bool fs_write_file(std::string_view name, std::string_view data) override {
        bool ok = storRef().write(std::string(name).c_str(),
                                  reinterpret_cast<const uint8_t*>(data.data()), data.size());
        rt_.log().info("AppStorage", ok ? "write ok" : "write FAIL",
                       {{"app", appId_}, {"file", std::string(name)}, {"bytes", std::to_string(data.size())}});
        return ok;
    }

    std::vector<std::string> fs_list_files() override {
        return storRef().list();
    }

    bool fs_remove_file(std::string_view name) override {
        return storRef().remove(std::string(name).c_str());
    }

    uint64_t fs_bytes_used() override {
        return static_cast<uint64_t>(storRef().usedBytes());
    }

    // ── nema:net/http ─────────────────────────────────────────────────

    NemaResult<HttpResponse, std::string> http_get(std::string_view url) override {
        auto* client = rt_.container().resolve<nema::IHttpClient>();
        if (!client) return {false, {}, "http not available"};
        auto r = client->get(std::string(url).c_str());
        HttpResponse resp{static_cast<uint16_t>(r.status), std::move(r.body)};
        if (!r.ok()) return {false, resp, "transport error"};
        return {true, std::move(resp), {}};
    }

    NemaResult<HttpResponse, std::string> http_post(std::string_view, std::string_view, std::string_view) override {
        return {false, {}, "http post not yet implemented"};
    }

    // ── nema:net/wifi ─────────────────────────────────────────────────
    // @future — stubs

    bool wifi_is_connected() override { return false; }
    std::string wifi_ssid() override { return {}; }
    std::string wifi_ip() override { return {}; }
    std::vector<WifiAp> wifi_scan() override { return {}; }
    NemaResult<void, std::string> wifi_connect(std::string_view, std::string_view) override {
        return {false, "wifi not implemented"};
    }
    void wifi_disconnect() override {}

    // ── nema:wifi/radio (Plan 87 Fase 4) ─────────────────────────────────────
    // All methods delegate to IRadioWifi (SimWifiRadio / Esp32WifiRadio).

    nema::IRadioWifi* radio() { return rt_.container().resolve<nema::IRadioWifi>(); }

    NemaResult<std::vector<ScanResult>, std::string> radio_scan() override {
        auto* r = radio();
        if (!r) return {false, {}, "no_radio"};
        const auto raw = r->scan();
        std::vector<ScanResult> out;
        out.reserve(raw.size());
        for (const auto& s : raw) {
            ScanResult res;
            res.bssid   = s.bssid;
            res.ssid    = s.ssid;
            res.channel = s.channel;
            res.rssi    = s.rssi;
            res.auth    = s.auth;
            out.push_back(std::move(res));
        }
        return {true, std::move(out), {}};
    }

    // monitor_open / monitor_read / monitor_close / inject — Fase 5 stubs.
    NemaResult<void, std::string> radio_monitor_open(uint8_t ch) override {
        auto* r = radio();
        if (!r || !r->monitorOpen(ch)) return {false, "not_supported"};
        return {true, {}};
    }
    NemaResult<std::vector<uint8_t>, std::string> radio_monitor_read(uint32_t max) override {
        auto* r = radio();
        if (!r) return {false, {}, "no_radio"};
        std::vector<uint8_t> buf(max);
        int n = r->monitorRead(buf.data(), max, 500);
        if (n <= 0) return {false, {}, "empty"};
        buf.resize(static_cast<size_t>(n));
        return {true, std::move(buf), {}};
    }
    NemaResult<void, std::string> radio_monitor_close() override {
        if (auto* r = radio()) r->monitorClose();
        return {true, {}};
    }
    NemaResult<void, std::string> radio_inject(uint8_t ch, const std::vector<uint8_t>& frame) override {
        auto* r = radio();
        if (!r || !r->inject(ch, frame.data(), frame.size()))
            return {false, "not_supported"};
        return {true, {}};
    }

    // deauth/beacon-spam/wait-event removed (Plan 91): apps build attack frames
    // and inject() them themselves; captive portals use apStart()+sockets. The
    // host no longer exposes attack verbs or the matured-event ring.

    // ── nema:profile ──────────────────────────────────────────────────

    std::string profile_user_name() override {
        auto* p = rt_.container().resolve<nema::ProfileService>();
        return p ? p->userName() : "";
    }

    std::string profile_device_name() override {
        auto* p = rt_.container().resolve<nema::ProfileService>();
        return p ? p->deviceName() : "";
    }

    bool profile_has_password() override {
        auto* p = rt_.container().resolve<nema::ProfileService>();
        return p ? p->hasPassword() : false;
    }

    bool profile_verify_password(std::string_view input) override {
        auto* p = rt_.container().resolve<nema::ProfileService>();
        return p ? p->verifyPassword(std::string(input)) : false;
    }

    // ── nema:bt/ble ───────────────────────────────────────────────────
    // @future — stubs

    NemaResult<void, std::string> ble_enable() override { return {false, "ble not implemented"}; }
    void ble_disable() override {}
    bool ble_is_enabled() override { return false; }

    // ── nema:media/* ──────────────────────────────────────────────────
    // @future — stubs

    std::vector<std::string> audio_input_list() override { return {}; }
    std::vector<std::string> audio_output_list() override { return {}; }
    std::vector<std::string> camera_list() override { return {}; }
    NemaResult<std::string, std::string> camera_capture() override { return {false, {}, "camera not implemented"}; }

    // ── nema:input ────────────────────────────────────────────────────
    // @future — stubs

    std::string input_hint(std::string_view) override { return {}; }
    std::vector<std::string> input_actions() override { return {}; }

    // ── aether:ui ─────────────────────────────────────────────────────
    // Delegates to aether_abi.cpp; arena must be set by ComponentApp::run()
    // via aether_set_arena() before build() calls these (Plan 50/52).

    int32_t view_view_begin(std::string_view direction) override {
        std::string d(direction);
        return (int32_t)(intptr_t)aether_view_begin(d.c_str());
    }
    void    view_view_end() override { aether_view_end(); }

    int32_t text_label(std::string_view content) override {
        std::string s(content);
        return (int32_t)(intptr_t)aether_text_label(s.c_str());
    }
    int32_t text_styled(std::string_view content, std::string_view variant) override {
        std::string c(content), v(variant);
        return (int32_t)(intptr_t)aether_text_styled(c.c_str(), v.c_str());
    }
    int32_t interactive_button(std::string_view label, int32_t on_press) override {
        std::string l(label);
        return (int32_t)(intptr_t)aether_interactive_button(l.c_str(), on_press);
    }
    int32_t scroll_scroll_begin() override {
        return (int32_t)(intptr_t)aether_scroll_begin();
    }
    void    scroll_scroll_end() override { aether_scroll_end(); }

private:
    nema::Runtime& rt_;
    std::string    appId_;
    std::string    ns_;    // 8-char djb2 hash of appId_ — fits NVS 15-char namespace limit

    // When set (UI app path), storage() is pre-warmed on the GUI thread and safe
    // to call from PSRAM-stacked app threads. When null (CLI path), stor() lazily
    // constructs an AppStorage on the spot (CLI threads use internal-RAM stacks).
    nema::AppContext*              appCtx_ = nullptr;
    std::optional<nema::AppStorage> stor_;

    nema::AppStorage& storRef() {
        if (appCtx_) return appCtx_->storage();
        if (!stor_)  stor_.emplace(appId_, rt_.fs(), rt_.config(), false);
        return *stor_;
    }
};

} // anon namespace

// ── Factory ────────────────────────────────────────────────────────────────

HostApi* createNemaHost(nema::Runtime& rt, std::string appId) {
    return new NemaHostImpl(rt, std::move(appId));
}

HostApi* createNemaHost(nema::Runtime& rt, std::string appId, nema::AppContext* ctx) {
    return new NemaHostImpl(rt, std::move(appId), ctx);
}
