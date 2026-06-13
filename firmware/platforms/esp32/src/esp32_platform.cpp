#include "nema/system/capabilities.h"
#include "nema/esp32/esp32_platform.h"
#include "nema/runtime.h"
#include "nema/board.h"
#include "nema/config/config_store.h"
#include "nema/service/service_container.h"
#include "nema/system/capability_registry.h"
#include "nema/hal/display.h"
#include "nema/log/logger.h"
#include "nema/services/remote_service.h"
#include "nema/services/cli_service.h"
#include "nema/apps/js_app_store.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <string>
#include <vector>

namespace nema {

void Esp32Platform::registerDrivers(Runtime& rt) {
    rt_ = &rt;
    // Each driver self-registers via its lifecycle hook (deps, service, caps, hw).
    wifi_.onRegister(rt);
    http_.onRegister(rt);
    ble_.onRegister(rt);   // BLE radio (NimBLE) — capability "bluetooth.ble"

    // NVS-backed config store — available to all ESP32 boards.
    config_.init(rt.log());
    rt.container().registerService(&config_);
    rt.container().registerAs<IConfigStore>(&config_);

    // Owner profile: load from NVS (or seed defaults on first boot).
    profile_.init(config_);
    rt.container().registerService(&profile_);
    rt.capabilities().add(caps::Profile);
}

void Esp32Platform::postRegister(Runtime& rt) {
    // --- SUBSTRATE (Plan 42 Fase 3): the KLP transport + CLI + VFS come up
    // UNCONDITIONALLY, independent of any display. A headless board (no display
    // capability) is still fully usable over USB-CDC/BLE via the CLI — the
    // console is the substrate, the screen is just an optional consumer on top.
    // KLP rides a MUX of transports: USB-CDC (always — native USB) + BLE (if the
    // radio is present). The host reaches the device over whichever connects.
    usbCdc_.start();                 // reader task on the USB-CDC (Serial)
    usbLink_.init(usbCdc_);
    mux_.add(&usbLink_);             // USB cable
    rt.capabilities().add(caps::RemoteUsb);
    if (rt.capabilities().has(caps::BtBle)) {
        cable_.init(ble_);          // KLP GATT TX/RX on the radio
        mux_.add(&cable_);          // BLE cable
    }
    link_.attach(&mux_, LinkService::Role::Device);

    remote_.init(link_, rt.input());                    // INPUT/SYSTEM dispatch
    remote_.attachLog(rt.log());                        // stream logs on LOG channel
    remote_.attachEvents(rt.events());                  // stream events on EVENT channel
    remote_.onPower(&Esp32Platform::powerThunk, this);
    remote_.onControl(&Esp32Platform::controlThunk, this);   // OTA app-install (Plan 37)
    remote_.setProfile(rt.board().profile());
    // Advertise the owner's chosen device name over BLE (visible in scans).
    if (auto* bt = rt.container().resolve<IBluetoothController>())
        bt->setDeviceName(profile_.deviceName().c_str());

    // CLI terminal over KLP (Plan 40). Core built-ins + a live-heap `ram` that
    // replaces the totals-only core version with real free-heap numbers.
    registerCoreCliCommands(cli_, rt);
    cli_.add("ram", "free heap / PSRAM (live)",
        [](const std::vector<std::string>&, const CliService::Out& out) {
            out("free heap:  " + std::to_string(esp_get_free_heap_size()) + " B");
            out("min free:   " + std::to_string(esp_get_minimum_free_heap_size()) + " B");
            out("free psram: " + std::to_string(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) + " B");
        });
    remote_.attachCli(cli_);

    // VFS + FILE channel. Root "/" is PERSISTENT (LittleFS on the internal flash
    // "spiffs" partition — survives reboot); "/tmp" is volatile RAM scratch. A FAT
    // backend can mount at "/sd" when a card is detected — the mount table means no
    // upper layer changes. (Plan 38.)
    bool fsOk = rootFs_.begin("spiffs", "/lfs");
    vfs_.mount("/", &rootFs_);
    vfs_.mount("/tmp", &tmpFs_);
    rt.container().registerAs<IFileSystem>(&vfs_);
    rt.capabilities().add(caps::Storage);
    // Liveness (Plan 42): /tmp RAM scratch always mounts, so storage is usable
    // even if the persistent root failed; report Fault when the root didn't mount.
    rt.capabilities().setState(caps::Storage,
                               fsOk ? ResourceState::Available : ResourceState::Fault);
    if (fsOk) {
        // Seed once on a fresh filesystem; never clobber the user's files on later
        // boots (this is the whole point of persistence).
        rootFs_.mkdir("/apps");
        rootFs_.mkdir("/data");
        std::vector<uint8_t> probe;
        if (!rootFs_.read("/readme.txt", probe)) {
            std::string msg = "Palanu filesystem (LittleFS — persistent across reboots).\n";
            rootFs_.write("/readme.txt", (const uint8_t*)msg.data(), msg.size());
        }
    }
    rt.log().info("Esp32Platform", "filesystem", {{"root", fsOk ? "littlefs" : "FAILED"}});
    remote_.attachFs(vfs_);

    // --- DISPLAY-ONLY: mirror the screen to the host (RemoteScreenTap). Only
    // wired when the board actually has a display; otherwise the substrate above
    // stands alone (headless = CLI over USB/BLE, no screen channel).
    auto* disp = rt.capabilities().has(caps::Display)
                     ? rt.container().resolve<IDisplayDriver>()
                     : nullptr;
    if (disp) {
        tap_.init(*disp, link_);                            // decorate board display
        rt.container().registerAs<IDisplayDriver>(&tap_);   // Canvas renders into tap
        link_.onReady(&Esp32Platform::readyThunk, this);    // push screen on connect
    }

    remoteWired_ = true;
    rt.log().info("Esp32Platform", "KLP remote wired",
                  {{"usb", "1"},
                   {"ble", rt.capabilities().has(caps::BtBle) ? "1" : "0"},
                   {"screen", disp ? "1" : "0"}});
}

void Esp32Platform::readyThunk(void* user) {
    static_cast<Esp32Platform*>(user)->tap_.requestResend();
}

void Esp32Platform::powerThunk(void* user, uint8_t op) {
    auto* s = static_cast<Esp32Platform*>(user);
    if (!s->rt_) return;
    if (op == SysOp::Restart)       s->rt_->requestRestart();
    else if (op == SysOp::Shutdown) s->rt_->requestShutdown();
}

void Esp32Platform::controlThunk(void* user, uint8_t op, const uint8_t* data, size_t len) {
    auto* s = static_cast<Esp32Platform*>(user);
    if (op == ExtOp::AppInstall && s->rt_)   // OTA: install a pushed .kapp live (Plan 37)
        JsAppStore::instance().installKapp(*s->rt_, (const char*)data, len);
}

void Esp32Platform::idle() {
    vTaskDelay(pdMS_TO_TICKS(5));
}

} // namespace nema
