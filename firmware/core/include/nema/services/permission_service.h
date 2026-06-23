#pragma once
#include "nema/service.h"
#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <cstdint>

namespace nema {

class Runtime;
class ViewDispatcher;

// PermissionService — persistent per-app capability grants (Plan 87 Fase 1).
//
// Stores grants via IConfigStore (namespace "perm", key = djb2(appId:cap) → 8 hex
// chars) so any bundle ID/cap length fits within NVS's 15-char key limit.
//
// Thread-safety model:
//   status() / request() — callable from any app thread.
//   guiTick()            — MUST be called from the GUI thread each frame.
//
// Decoupling: the permission screen itself lives in the display layer (aether).
// The caller registers a ScreenFactory via setScreenFactory() at boot — the
// factory receives the pending request + ViewDispatcher and handles push/pop.
// This avoids a circular core↔aether link dependency.
class PermissionService : public IService {
public:
    struct PermRequest {
        std::string appId;
        std::string cap;
        std::mutex  mu;
        std::condition_variable cv;
        uint8_t result = 2;   // default = denied (safe)
        bool    done   = false;

        void resolve(uint8_t r) {
            std::unique_lock<std::mutex> g(mu);
            result = r;
            done   = true;
            cv.notify_all();
        }
    };

    // Called by the display layer at boot to wire up the UI screen.
    // The factory is invoked on the GUI thread when a request arrives.
    using ScreenFactory = std::function<void(std::shared_ptr<PermRequest>, ViewDispatcher&)>;
    void setScreenFactory(ScreenFactory f) { factory_ = std::move(f); }

    void init(Runtime& rt);

    // ── App-thread API ───────────────────────────────────────────────────────

    // Returns 0=not_asked, 1=granted, 2=denied.
    uint8_t status(const std::string& appId, const std::string& cap) const;

    // If already decided, returns immediately. Otherwise blocks until the user
    // responds to the permission screen. Returns 1=granted, 2=denied.
    // A "skip" result (0, from pressing Back on the dialog) is NOT persisted —
    // the next request() call will show the dialog again.
    uint8_t request(const std::string& appId, const std::string& cap);

    // Revoke a previously granted permission — resets to 0 (not_asked) so
    // the next request() will show the dialog again rather than auto-deny.
    void revoke(const std::string& appId, const std::string& cap);

    // Grant a capability directly (e.g. from Settings toggle when status==denied).
    void grant(const std::string& appId, const std::string& cap);

    // ── GUI-thread API ───────────────────────────────────────────────────────

    // Call from GuiService loop each frame. Invokes the ScreenFactory when a
    // pending request is waiting and the screen hasn't been pushed yet.
    void guiTick(ViewDispatcher& vd);

    // ── IService ─────────────────────────────────────────────────────────────
    const char* name() const override { return "PermissionService"; }
    void start() override {}
    void stop()  override {}

private:
    void persist(const std::string& appId, const std::string& cap, uint8_t val);

    Runtime*      rt_ = nullptr;
    ScreenFactory factory_;

    std::mutex                    mu_;
    std::shared_ptr<PermRequest>  pending_;
    bool                          screenPushed_ = false;
};

} // namespace nema
