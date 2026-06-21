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
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <esp_cpu.h>
#include <esp_sntp.h>
#include <esp_heap_caps.h>
#include <esp_event.h>
#include <mdns.h>
#include "nema/event/event_bus.h"
#include "nema/event/event.h"
#include <string>
#include <vector>
#include <ctime>

namespace nema {

void Esp32Platform::registerDrivers(Runtime& rt) {
    rt_ = &rt;
    // Each driver self-registers via its lifecycle hook (deps, service, caps, hw).
    wifi_.onRegister(rt);
    http_.onRegister(rt);
    ble_.onRegister(rt);   // BLE radio (NimBLE) — capability "bluetooth.ble"
    usbHid_.onRegister(rt);

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
    // --- SUBSTRATE (Plan 42 Fase 3): the PLP transport + CLI + VFS come up
    // UNCONDITIONALLY, independent of any display. A headless board (no display
    // capability) is still fully usable over USB-CDC/BLE via the CLI — the
    // console is the substrate, the screen is just an optional consumer on top.
    // PLP rides a MUX of transports: USB-CDC (always — native USB) + BLE (if the
    // radio is present). The host reaches the device over whichever connects.
    usbCdc_.start();                 // reader task on the USB-CDC (Serial)
    usbLink_.init(usbCdc_);
    mux_.add(&usbLink_);             // USB cable
    rt.capabilities().add(caps::RemoteUsb);
    if (rt.capabilities().has(caps::BtBle)) {
        cable_.init(ble_);          // PLP GATT TX/RX on the radio
        mux_.add(&cable_);          // BLE cable
    }
    // PLP over WebSocket (Plan 75): Forge web (browser) reaches the device over
    // WiFi. The httpd only runs while online — bound on NetworkConnected, torn
    // down on NetworkDisconnected, so no port is open when offline.
    if (rt.capabilities().has(caps::NetWifi)) {
        wsLink_.init(&rt.log());
        mux_.add(&wsLink_);
        rt.capabilities().add(caps::RemoteNet);
        rt.capabilities().setState(caps::RemoteNet, ResourceState::Absent);  // up when online
        Runtime* rtp = &rt;
        rt.events().subscribe(events::NetworkConnected, [this, rtp](const Event&) {
            wsLink_.begin(8477);
            rtp->capabilities().setState(caps::RemoteNet, ResourceState::Available);
            // mDNS so `<board>.local` resolves and Forge can discover the device.
            if (!mdnsStarted_ && mdns_init() == ESP_OK) {
                mdns_hostname_set(rtp->board().name());
                mdns_instance_name_set("Palanu");
                mdns_txt_item_t txt[] = { {"path", "/plp"}, {"authreq", "0"} };
                mdns_service_add(nullptr, "_palanu", "_tcp", 8477, txt, 2);
                mdnsStarted_ = true;
            }
        });
        rt.events().subscribe(events::NetworkDisconnected, [this, rtp](const Event&) {
            wsLink_.end();
            rtp->capabilities().setState(caps::RemoteNet, ResourceState::Absent);
        });
    }
    link_.attach(&mux_, LinkService::Role::Device);

    authStore_.init(config_);                           // session auth (Plan 74)
    rt.container().registerService(&authStore_);        // resolvable by Settings->Remote
    remote_.init(link_, rt.input());                    // INPUT/SYSTEM dispatch
    remote_.attachAuth(authStore_);                     // gate privileged channels
    remote_.attachLog(rt.log());                        // stream logs on LOG channel
    remote_.attachEvents(rt.events());                  // stream events on EVENT channel
    remote_.onPower(&Esp32Platform::powerThunk, this);
    remote_.onControl(&Esp32Platform::controlThunk, this);   // OTA app-install (Plan 37)
    remote_.setProfile(rt.board().profile());
    // Advertise the owner's chosen device name over BLE (visible in scans).
    if (auto* bt = rt.container().resolve<IBluetoothController>())
        bt->setDeviceName(profile_.deviceName().c_str());

    // CLI terminal over PLP (Plan 40). Core built-ins + a live-heap `ram` that
    // replaces the totals-only core version with real free-heap numbers.
    registerCoreCliCommands(cli_, rt);
    cli_.add("ram", "free heap / PSRAM (live)",
        [](CliContext& c) {
            const auto& out = c.out;
            out("free heap:  " + std::to_string(esp_get_free_heap_size()) + " B");
            out("min free:   " + std::to_string(esp_get_minimum_free_heap_size()) + " B");
            out("free psram: " + std::to_string(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) + " B");
        });
    remote_.attachCli(cli_);
    remote_.attachSessions(rt.cliSessions());   // multi-session shells (Plan 45)
    rt.setCli(cli_);                            // expose to FbconServer local console

    // Firmware OTA (Plan 39): register the esp_ota updater + route the PLP Ota
    // channel to it. We've booted far enough to register services, so confirm the
    // running image is good — cancels any pending A/B rollback.
    rt.container().registerAs<IOtaUpdater>(&otaUpdater_);
    remote_.attachOta(otaUpdater_);
    otaUpdater_.confirmBoot();

    // VFS + FILE channel. Root "/" is PERSISTENT (LittleFS on the internal flash
    // "spiffs" partition — survives reboot); "/tmp" is volatile RAM scratch. A FAT
    // backend can mount at "/sd" when a card is detected — the mount table means no
    // upper layer changes. (Plan 38.)
    bool fsOk = rootFs_.begin("spiffs", "/lfs");
    vfs_.mount("/", &rootFs_);
    vfs_.mount("/tmp", &tmpFs_);
    rt.container().registerAs<IFileSystem>(&vfs_);
    rt.setFs(&vfs_);
    rt.capabilities().add(caps::Storage);
    // Liveness (Plan 42): /tmp RAM scratch always mounts, so storage is usable
    // even if the persistent root failed; report Fault when the root didn't mount.
    rt.capabilities().setState(caps::Storage,
                               fsOk ? ResourceState::Available : ResourceState::Fault);
    if (fsOk) {
        // Plan 83: one-time migration /badusb/ → /data/com.palanu.badusb/
        // (old path from pre-83 firmware; rename preserves user scripts)
        {
            std::vector<FsEntry> probe;
            bool oldExists = rootFs_.list("/badusb", probe);
            std::vector<FsEntry> newProbe;
            bool newExists = rootFs_.list("/data/com.palanu.badusb", newProbe);
            if (oldExists && !newExists) {
                rt.log().info("Esp32Platform", "migrating /badusb → /data/com.palanu.badusb");
                rootFs_.mkdir("/data");
                rootFs_.rename("/badusb", "/data/com.palanu.badusb");
            }
        }

        // Seed directory structure. mkdir() is idempotent — safe on every boot.
        rootFs_.mkdir("/system");
        rootFs_.mkdir("/system/assets");
        rootFs_.mkdir("/system/assets/anims");
        rootFs_.mkdir("/apps");
        rootFs_.mkdir("/data");
        rootFs_.mkdir("/data/com.palanu.badusb");

        std::string demo = "REM Hello World — BadUSB demo\n"
                           "DELAY 500\n"
                           "GUI r\n"
                           "DELAY 300\n"
                           "STRING notepad\n"
                           "ENTER\n"
                           "DELAY 500\n"
                           "STRINGLN Hello from Palanu BadUSB!\n";
        // Factory scripts — always written so they update after firmware upgrades.
        rootFs_.write("/data/com.palanu.badusb/demo.dd",
                      (const uint8_t*)demo.data(), demo.size());
        std::string rickroll =
            "REM Rickroll Mac — CMD+Space, Terminal, open Brave, fullscreen\n"
            "DELAY 500\n"
            "GUI SPACE\n"
            "DELAY 300\n"
            "STRING terminal\n"
            "ENTER\n"
            "DELAY 500\n"
            "STRING open -a \"Brave Browser\" https://youtu.be/dQw4w9WgXcQ\n"
            "ENTER\n"
            "DELAY 2000\n"
            "STRING f\n";
        rootFs_.write("/data/com.palanu.badusb/rickroll_mac.dd",
                      (const uint8_t*)rickroll.data(), rickroll.size());
        std::vector<uint8_t> probe;
        if (!rootFs_.read("/readme.txt", probe)) {
            std::string msg = "Palanu filesystem (LittleFS — persistent across reboots).\n";
            rootFs_.write("/readme.txt", (const uint8_t*)msg.data(), msg.size());
        }
    }
    rt.log().info("Esp32Platform", "filesystem", {{"root", fsOk ? "littlefs" : "FAILED"}});

    // microSD (FAT) — only on boards that wire an SD socket (Plan 38). Non-fatal:
    // a missing card or failed mount just means "/sd" is absent; boot continues.
    // Once mounted, the VFS auto-surfaces "sd" when listing "/", so the File
    // Browser shows it with no extra wiring.
    SdSpiConfig sdCfg;
    if (rt.board().sdSpi(sdCfg)) {
        if (sdFs_.begin(sdCfg)) {
            vfs_.mount("/sd", &sdFs_);
            rt.capabilities().add(caps::Storage);
            rt.log().info("Esp32Platform", "microSD mounted", {{"path", "/sd"}});
        } else {
            rt.log().warn("Esp32Platform", "microSD not mounted (no card / mount failed)",
                          {{"cs", std::to_string(sdCfg.cs)}, {"sclk", std::to_string(sdCfg.sclk)}});
        }
    }

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
        remote_.onReady(&Esp32Platform::readyThunk, this);  // push screen on connect (after auth)
    }

    remoteWired_ = true;
    rt.log().info("Esp32Platform", "PLP remote wired",
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
    // Route through the runtime so the remote path and the CLI `power` command
    // converge on one place (Runtime::request* → IPlatform::power below).
    if (op == SysOp::Restart)         s->rt_->requestRestart();
    else if (op == SysOp::Shutdown)   s->rt_->requestShutdown();
    else if (op == SysOp::Bootloader) s->rt_->requestBootloader();
}

// Actually drive the hardware. On ESP32 the Arduino loop() just calls rt.step()
// forever and never reads shutdownRequested_, so the host/sim "exit the run()
// loop" pattern is a no-op here — we reboot/sleep the chip directly. The short
// delay lets a pending reply (the OTA End-Ok ack, or the CLI "restarting…" line)
// flush over USB/BLE before the link drops, so the host sees a clean finish.
void Esp32Platform::power(PowerAction action) {
    if (rt_) rt_->log().info("Esp32Platform",
                             action == PowerAction::Restart ? "rebooting"
                             : action == PowerAction::Bootloader ? "entering bootloader"
                             : "powering off");
    vTaskDelay(pdMS_TO_TICKS(200));
    if (action == PowerAction::Restart) esp_restart();
    else if (action == PowerAction::Bootloader) {
        // CPU-only reset preserves GPIO state through reset, unlike
        // esp_restart() which does a system reset that clears the GPIO matrix.
        // The ROM bootloader reads GPIO0 after reset — LOW = download mode.
        gpio_reset_pin(GPIO_NUM_0);
        gpio_set_direction(GPIO_NUM_0, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_0, 0);
        esp_cpu_reset(0);
        while (1);
    }
    else esp_deep_sleep_start();
}

void Esp32Platform::controlThunk(void* user, uint8_t op, const uint8_t* data, size_t len) {
    auto* s = static_cast<Esp32Platform*>(user);
    if (op == ExtOp::AppInstall && s->rt_)   // OTA: install a pushed .papp live (Plan 37)
        JsAppStore::instance().installPappBytes(*s->rt_, (const char*)data, len);
}

void Esp32Platform::idle() {
    vTaskDelay(pdMS_TO_TICKS(5));
}

bool Esp32Platform::syncNtp() {
    if (!rt_) return false;

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    int retry = 0;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }

    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        time_t now;
        time(&now);
        rt_->clock().setEpochMs((uint64_t)now * 1000);
        return true;
    }
    return false;
}

} // namespace nema
