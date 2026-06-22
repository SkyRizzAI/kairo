#pragma once
#include "nema/platform.h"
#include "nema/wasm/wasm_clock.h"
#include "nema/wasm/wasm_config.h"
#include "nema/wasm/null_display.h"
#include "nema/wasm/wasm_cable_transport.h"
#include "nema/link/link_service.h"
#include "nema/hal/remote_screen_tap.h"
#include "nema/services/remote_service.h"
#include "nema/wasm/sim_ota_updater.h"
#include "nema/services/cli_service.h"
#include "nema/services/profile_service.h"
#include "nema/services/storage_service.h"
#include "nema/services/permission_service.h"
#include "nema/services/resource_broker.h"
#include "nema/services/system_wifi_manager.h"
#include "nema/fs/mem_filesystem.h"
#include "nema/fs/vfs.h"
#include "nema/sim/sim_wifi_driver.h"
#include "nema/sim/sim_wifi_radio.h"
#include "nema/wasm/sim_secure_element.h"

namespace nema {

// WASM platform — the firmware's environment in the browser. No glass, no stdio:
// the display is a RemoteScreenTap streaming over the virtual cable, input comes
// from the cable, logs go out on the cable. The device IS a PLP endpoint.
class WasmPlatform : public IPlatform {
public:
    const char* name() const override { return "wasm"; }
    IClock& clock() override { return clock_; }
    void registerDrivers(Runtime& rt) override;
    void idle() override {}

private:
    static void powerThunk(void* user, uint8_t op);
    static void readyThunk(void* user);
    static void controlThunk(void* user, uint8_t op, const uint8_t* data, size_t len);

    WasmClock          clock_;
    WasmConfig         config_;
    NullDisplay        display_;
    WasmCableTransport cable_;
    LinkService        link_;
    RemoteScreenTap    tap_;
    RemoteAuthStore    authStore_;   // session auth policy (Plan 74)
    RemoteService      remote_;
    SimOtaUpdater      otaUpdater_;   // OTA dry-run for in-browser flow testing (Plan 39)
    CliService         cli_;
    ProfileService     profile_;   // owner identity (Plan 40)
    StorageService     storage_;   // app data routing + management (Plan 83)
    PermissionService  permSvc_;   // per-app capability grants (Plan 87)
    ResourceBroker     broker_;    // exclusive HW leases + auto-release (Plan 87)
    SystemWifiManager  sysWifi_;   // system WiFi lease + suspend/restore (Plan 87)
    Vfs                vfs_;       // mount table (root + demo /sd)
    MemFileSystem      rootFs_;    // mounted at "/"
    MemFileSystem      sdFs_;      // mounted at "/sd" (demonstrates a 2nd partition)
    SimWifiDriver      wifi_;
    SimWifiRadio       wifiRadio_;  // raw radio access (Plan 87 Fase 4)
    SimSecureElement   secure_;    // software-emulated SE050 (Plan: crypto wallet)
    Runtime*           rt_ = nullptr;
};

} // namespace nema
