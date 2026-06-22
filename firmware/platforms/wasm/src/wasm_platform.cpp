#include "nema/system/capabilities.h"
#include "nema/wasm/wasm_platform.h"
#include "nema/runtime.h"
#include "nema/service/service_container.h"
#include "nema/services/input_service.h"
#include "nema/system/hardware_registry.h"
#include "nema/system/capability_registry.h"
#include "nema/board.h"
#include "nema/hal/display.h"
#include "nema/hal/wifi.h"
#include "nema/config/config_store.h"
#include "nema/event/event_bus.h"
#include "nema/apps/js_app_store.h"
#include "nema/app/papp_installer.h"
#include "nema/assets/anims/dolphin_sleep_panim.h"
#include "nema/assets/fonts/iosk_mono_pack.h"
#include "nema/assets/fonts/iosk_cond_pack.h"
#include <string>
#include <vector>
#include <cstdlib>

namespace nema {

void WasmPlatform::registerDrivers(Runtime& rt) {
    rt_ = &rt;

    cable_.init();
    link_.attach(&cable_, LinkService::Role::Device);
    tap_.init(display_, link_);

    rt.container().registerService(&display_);          // NullDisplay (IService)
    rt.container().registerAs<IDisplayDriver>(&tap_);    // Canvas renders into the tap → streamed
    rt.container().registerAs<IConfigStore>(&config_);

    // Owner profile: load from config (or seed defaults).
    profile_.init(config_);
    rt.container().registerService(&profile_);
    rt.capabilities().add(caps::Profile);

    // WiFi (virtual router) — same SimWifiDriver as the native sim, so the
    // device's WiFi app works and Forge's WiFi panel can inject networks.
    wifi_.init(rt.log(), rt.events(), &rt.asyncPoster());
    rt.container().registerService(&wifi_);
    rt.container().registerAs<IWifiDriver>(&wifi_);
    rt.hardware().add({"wifi", DriverKind::Wifi, "virtual"});
    rt.capabilities().add(caps::NetWifi);

    authStore_.init(config_);                         // session auth (Plan 74)
    rt.container().registerService(&authStore_);
    remote_.init(link_, rt.input());
    remote_.attachAuth(authStore_);
    remote_.attachLog(rt.log());
    remote_.attachEvents(rt.events());                // stream events → EVENT channel
    remote_.onPower(&WasmPlatform::powerThunk, this);
    remote_.onControl(&WasmPlatform::controlThunk, this);
    remote_.setProfile(rt.board().profile());

    // CLI terminal over PLP (Plan 40) — same built-ins as hardware; commands that
    // need a missing driver (e.g. `ble`) report "not available" in the browser.
    registerCoreCliCommands(cli_, rt);
    remote_.attachCli(cli_);
    remote_.attachSessions(rt.cliSessions());   // multi-session shells (Plan 45)
    rt.setCli(cli_);                            // expose to FbconServer local console
    // OTA dry-run: lets the Forge "Update firmware" flow + PLP Ota protocol be
    // exercised in-browser (no real image swap — WASM has no flash). Plan 39.
    rt.container().registerAs<IOtaUpdater>(&otaUpdater_);
    remote_.attachOta(otaUpdater_);

    // VFS with two in-RAM partitions to show the Linux-style mount system: root
    // at "/", plus a second backend mounted at "/sd" (a stand-in for a microSD —
    // writes there land in a DIFFERENT backend). On hardware these become LittleFS
    // (/) and FAT (/sd). The FILE channel/browser see one tree regardless.
    vfs_.mount("/", &rootFs_);
    vfs_.mount("/sd", &sdFs_);
    rt.container().registerAs<IFileSystem>(&vfs_);
    rt.setFs(&vfs_);
    rt.capabilities().add(caps::Storage);
    rootFs_.seed("/readme.txt", "Palanu virtual filesystem (in-RAM, volatile).\n"
                                "See examples/ folder for sample apps.\n"
                                "Copy .papp folders to /apps/ or /sd/apps/.\n");
    // Seed desktop animation — laptop.panim (128×51, 8 frames, 6.4KB).
    rootFs_.mkdir("/system");
    rootFs_.mkdir("/system/assets");
    rootFs_.mkdir("/system/assets/anims");
    rootFs_.write("/system/assets/anims/laptop.panim", kDolphinSleepPanim, kDolphinSleepPanimLen);
    // Font packs — seeded so Settings > Appearances > Font can discover them.
    rootFs_.mkdir("/system/assets/fonts");
    rootFs_.mkdir("/system/assets/fonts/IoskeleyMono");
    rootFs_.write("/system/assets/fonts/IoskeleyMono/reg8.bmf",   kIoskMonoReg8,   kIoskMonoReg8Len);
    rootFs_.write("/system/assets/fonts/IoskeleyMono/bold8.bmf",  kIoskMonoBold8,  kIoskMonoBold8Len);
    rootFs_.write("/system/assets/fonts/IoskeleyMono/reg10.bmf",  kIoskMonoReg10,  kIoskMonoReg10Len);
    rootFs_.write("/system/assets/fonts/IoskeleyMono/bold10.bmf", kIoskMonoBold10, kIoskMonoBold10Len);
    rootFs_.write("/system/assets/fonts/IoskeleyMono/reg12.bmf",  kIoskMonoReg12,  kIoskMonoReg12Len);
    rootFs_.write("/system/assets/fonts/IoskeleyMono/bold12.bmf", kIoskMonoBold12, kIoskMonoBold12Len);
    rootFs_.mkdir("/system/assets/fonts/IoskeleyMono-Condensed");
    rootFs_.write("/system/assets/fonts/IoskeleyMono-Condensed/reg8.bmf",   kIoskCondReg8,   kIoskCondReg8Len);
    rootFs_.write("/system/assets/fonts/IoskeleyMono-Condensed/bold8.bmf",  kIoskCondBold8,  kIoskCondBold8Len);
    rootFs_.write("/system/assets/fonts/IoskeleyMono-Condensed/reg10.bmf",  kIoskCondReg10,  kIoskCondReg10Len);
    rootFs_.write("/system/assets/fonts/IoskeleyMono-Condensed/bold10.bmf", kIoskCondBold10, kIoskCondBold10Len);
    rootFs_.write("/system/assets/fonts/IoskeleyMono-Condensed/reg12.bmf",  kIoskCondReg12,  kIoskCondReg12Len);
    rootFs_.write("/system/assets/fonts/IoskeleyMono-Condensed/bold12.bmf", kIoskCondBold12, kIoskCondBold12Len);
    // Create empty scan + data roots
    rootFs_.mkdir("/system/apps");
    rootFs_.mkdir("/system/data");
    sdFs_.mkdir("/apps");       // appears as /sd/apps in VFS
    sdFs_.mkdir("/assets");     // appears as /sd/assets in VFS
    sdFs_.mkdir("/assets/anims");
    sdFs_.seed("/card.txt", "Drop .papp folders here. Scanned recursively.\n");

    // Storage routing service — must init after VFS is ready (rt.setFs called above).
    storage_.init(rt);
    rt.container().registerService(&storage_);

    // Permission grants (Plan 87 Fase 1) — init after config is registered.
    permSvc_.init(rt);
    rt.container().registerService(&permSvc_);

    // Exclusive HW leases + auto-release on exit (Plan 87 Fase 2).
    broker_.init(rt);
    rt.container().registerService(&broker_);

    remote_.attachFs(vfs_);

    remote_.onReady(&WasmPlatform::readyThunk, this); // push current screen on connect (after auth)

    // Secure element — software-emulated SE050 so the crypto-wallet feature can
    // be developed/tested in the browser. Apps gate on caps::Secure (ADR 0005).
    rt.container().registerAs<ISecureElement>(&secure_);
    rt.hardware().add({"secure", DriverKind::Other, "sim SE050 (software)"});
    rt.capabilities().add(caps::Secure);

    rt.hardware().add({"display", DriverKind::Display, "wasm 1-bit (remote)"});
    rt.capabilities().add(caps::Display);
    rt.capabilities().add(caps::Input);
    rt.capabilities().add(caps::Input2D);  // Forge sends arrow-key codes (Up/Down/Left/Right)
}

void WasmPlatform::controlThunk(void* user, uint8_t op, const uint8_t* data, size_t len) {
    auto* s = static_cast<WasmPlatform*>(user);
    if (op == ExtOp::AppInstall) {   // OTA: Forge pushed a .papp → install live (Plan 37)
        if (s->rt_) JsAppStore::instance().installPappBytes(*s->rt_, (const char*)data, len);
        return;
    }
    if (op == ExtOp::AppScan) {      // Plan 86 Fase 6: rescan VFS after zip install
        if (s->rt_) loadInstalledPapps(*s->rt_);
        return;
    }
    if (op != ExtOp::WifiSetNetworks) return;
    // data: "ssid\tpw\trssi\tonline\n..." (one network per line)
    std::string blob((const char*)data, len);
    std::vector<SimWifiDriver::SimNet> nets;
    size_t pos = 0;
    while (pos < blob.size()) {
        size_t nl = blob.find('\n', pos);
        std::string line = blob.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? blob.size() : nl + 1;
        if (line.empty()) continue;
        SimWifiDriver::SimNet n;
        size_t a = line.find('\t'), b = line.find('\t', a + 1), c = line.find('\t', b + 1);
        if (a == std::string::npos || b == std::string::npos || c == std::string::npos) continue;
        n.ssid = line.substr(0, a);
        n.password = line.substr(a + 1, b - a - 1);
        n.rssi = (int8_t)std::atoi(line.substr(b + 1, c - b - 1).c_str());
        n.online = line.substr(c + 1) != "0";
        if (!n.ssid.empty()) nets.push_back(std::move(n));
    }
    s->wifi_.setNetworks(std::move(nets));
}

void WasmPlatform::readyThunk(void* user) {
    static_cast<WasmPlatform*>(user)->tap_.requestResend();
}

void WasmPlatform::powerThunk(void* user, uint8_t op) {
    auto* s = static_cast<WasmPlatform*>(user);
    if (!s->rt_) return;
    if (op == SysOp::Restart)         s->rt_->requestRestart();
    else if (op == SysOp::Shutdown)   s->rt_->requestShutdown();
    else if (op == SysOp::Bootloader) s->rt_->requestBootloader();
}

} // namespace nema
