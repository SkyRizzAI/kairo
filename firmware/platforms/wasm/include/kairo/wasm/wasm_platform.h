#pragma once
#include "kairo/platform.h"
#include "kairo/wasm/wasm_clock.h"
#include "kairo/wasm/wasm_config.h"
#include "kairo/wasm/null_display.h"
#include "kairo/wasm/wasm_cable_transport.h"
#include "kairo/link/link_service.h"
#include "kairo/hal/remote_screen_tap.h"
#include "kairo/services/remote_service.h"
#include "kairo/services/cli_service.h"
#include "kairo/services/profile_service.h"
#include "kairo/fs/mem_filesystem.h"
#include "kairo/fs/vfs.h"
#include "kairo/sim/sim_wifi_driver.h"

namespace kairo {

// WASM platform — the firmware's environment in the browser. No glass, no stdio:
// the display is a RemoteScreenTap streaming over the virtual cable, input comes
// from the cable, logs go out on the cable. The device IS a KLP endpoint.
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
    RemoteService      remote_;
    CliService         cli_;
    ProfileService     profile_;   // owner identity (Plan 40)
    Vfs                vfs_;       // mount table (root + demo /sd)
    MemFileSystem      rootFs_;    // mounted at "/"
    MemFileSystem      sdFs_;      // mounted at "/sd" (demonstrates a 2nd partition)
    SimWifiDriver      wifi_;
    Runtime*           rt_ = nullptr;
};

} // namespace kairo
